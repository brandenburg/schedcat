#include <algorithm> // for greater
#include <queue>
#include <vector>

#include "tasks.h"
#include "schedulability.h"

#include "edf/baruah.h"

#include <iostream>
#include "task_io.h"

#include "cpu_time.h"

using namespace std;

const double BaruahGedf::MAX_RUNTIME = 5.0; /* seconds */


static void demand_bound_function(const Task &tsk,
                                  const integral_t &t,
                                  integral_t &db)
{
    db = t;
    db -= tsk.get_deadline();
    if (db >= 0)
    {
        db /= tsk.get_period();
        db += 1;
        db *= tsk.get_wcet();
    }
    else
        db = 0;
}

class DBFPointsOfChange
{
private:
    integral_t     cur;
    unsigned long pi; // period

public:
    void init(const Task& tsk_i, const Task& tsk_k)
    {
        init(tsk_k.get_deadline(), tsk_i.get_deadline(), tsk_i.get_period());
    }

    void init(unsigned long dk, unsigned long di, unsigned long pi)
    {
        this->pi = pi;

        // cur = di - dk (without underflow!)
        cur  = di;
        cur -= dk;
        while (cur < 0)
            next();
    }

    const integral_t& get_cur() const
    {
        return cur;
    }

    void next()
    {
        cur += pi;
    }
};

class DBFComparator {
public:
    bool operator() (DBFPointsOfChange *a, DBFPointsOfChange *b)
    {
        return b->get_cur() < a->get_cur();
    }
};

typedef priority_queue<DBFPointsOfChange*,
                       vector<DBFPointsOfChange*>,
                       DBFComparator> DBFQueue;

class AllDBFPointsOfChange
{
private:
    DBFPointsOfChange *dbf;
    DBFQueue           queue;
    integral_t          last;
    integral_t         *upper_bound;

public:
    void init(const TaskSet &ts, int k, integral_t* bound)
    {
        last = -1;
        dbf = new DBFPointsOfChange[ts.get_task_count()];
        for (unsigned int i = 0; i < ts.get_task_count(); i++)
        {
            dbf[i].init(ts[i], ts[k]);
            queue.push(dbf + i);
        }
        upper_bound = bound;
    }

    ~AllDBFPointsOfChange()
    {
        delete[] dbf;
    }

    bool get_next(integral_t &t)
    {
        if (last > *upper_bound)
            return false;

        DBFPointsOfChange* pt;
        do // avoid duplicates
        {
            pt = queue.top();
            queue.pop();
            t = pt->get_cur();
            pt->next();
            queue.push(pt);
        } while (t == last);
        last = t;

        return last <= *upper_bound;
    }
};

static
void interval1(unsigned int i, unsigned int k, const TaskSet &ts,
               const integral_t &ilen, integral_t &i1)
{
    integral_t dbf, tmp;
    tmp = ilen + ts[k].get_deadline();
    demand_bound_function(ts[i], tmp, dbf);
    if (i == k)
        i1 = min(integral_t(dbf - ts[k].get_wcet()), ilen);
    else
        i1 = min(dbf,
                 integral_t(ilen + ts[k].get_deadline() -
                           (ts[k].get_wcet() - 1)));
}


static void demand_bound_function_prime(const Task &tsk,
                                        const integral_t &t,
                                        integral_t &db)
// carry-in scenario
{
    db = t;
    db /= tsk.get_period();
    db *= tsk.get_wcet();
    db += min(integral_t(tsk.get_wcet()), integral_t(t % tsk.get_period()));
}

static void interval2(unsigned int i, unsigned int k, const TaskSet &ts,
                       const integral_t &ilen, integral_t &i2)
{
    integral_t dbf, tmp;

    tmp = ilen + ts[k].get_deadline();
    demand_bound_function_prime(ts[i], tmp, dbf);
    if (i == k)
        i2 = min(integral_t(dbf - ts[k].get_wcet()), ilen);
    else
        i2 = min(dbf,
                 integral_t(ilen + ts[k].get_deadline() -
                           (ts[k].get_wcet() - 1)));
}

class MPZComparator {
public:
    bool operator() (integral_t *a, integral_t *b)
    {
        return *b < *a;
    }
};

bool BaruahGedf::is_task_schedulable(unsigned int k,
                                     const TaskSet &ts,
                                     const integral_t &ilen,
                                     integral_t &i1,
                                     integral_t &sum,
                                     integral_t *idiff,
                                     integral_t **ptr)
{
    integral_t bound;
    sum = 0;

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        interval1(i, k, ts, ilen, i1);
        interval2(i, k, ts, ilen, idiff[i]);
        sum      += i1;
        idiff[i] -= i1;
    }

    /* sort pointers to idiff to find largest idiff values */
    sort(ptr, ptr + ts.get_task_count(), MPZComparator());

    for (unsigned int i = 0; i < m - 1 && i < ts.get_task_count(); i++)
        sum += *ptr[i];

    bound  = ilen + ts[k].get_deadline() - ts[k].get_wcet();
    bound *= m;
    return sum <= bound;
}

void BaruahGedf::get_max_test_points(const TaskSet &ts,
                                     fractional_t &m_minus_u,
                                     integral_t* maxp)
{
    unsigned long* wcet = new unsigned long[ts.get_task_count()];

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        wcet[i] = ts[i].get_wcet();

    sort(wcet, wcet + ts.get_task_count(), greater<unsigned long>());

    fractional_t u, tdu_sum;
    integral_t csigma, mc;

    csigma = 0;
    for (unsigned int i = 0; i < m - 1 && i < ts.get_task_count(); i++)
        csigma += wcet[i];

    tdu_sum = 0;
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        ts[i].get_utilization(u);
        tdu_sum += (ts[i].get_period() - ts[i].get_deadline()) * u;
    }

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        mc  = ts[i].get_wcet();
        mc *= m;
        maxp[i] = (csigma - (ts[i].get_deadline() * m_minus_u) + tdu_sum + mc)
            / m_minus_u;
    }

    delete[] wcet;
}

bool BaruahGedf::is_schedulable(const TaskSet &ts,
                                bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks() &&
              ts.is_not_overutilized(m) &&
              ts.has_only_constrained_deadlines() &&
              ts.has_no_self_suspending_tasks()))
            return false;

        if (ts.get_task_count() == 0)
            return true;
    }

    fractional_t m_minus_u;
    ts.get_utilization(m_minus_u);
    m_minus_u *= -1;
    m_minus_u += m;

    if (m_minus_u <= 0) {
        // Baruah's G-EDF test requires strictly positive slack.
        // In the case of zero slack the testing interval becomes
        // infinite. Therefore, we can't do anything but bail out.
        return false;
    }

    double start_time = get_cpu_usage();

    integral_t i1, sum;
    integral_t *max_test_point, *idiff;
    integral_t** ptr; // indirect access to idiff

    idiff          = new integral_t[ts.get_task_count()];
    max_test_point = new integral_t[ts.get_task_count()];
    ptr            = new integral_t*[ts.get_task_count()];
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        ptr[i] = idiff + i;

    get_max_test_points(ts, m_minus_u, max_test_point);

    integral_t ilen;
    bool point_in_range = true;
    bool schedulable = true;

    AllDBFPointsOfChange *all_pts;

    all_pts = new AllDBFPointsOfChange[ts.get_task_count()];
    for (unsigned int k = 0; k < ts.get_task_count(); k++)
        all_pts[k].init(ts, k, max_test_point + k);

    // for every task for which point <= max_ak
    unsigned long iter_count = 0;
    while (point_in_range && schedulable)
    {
        point_in_range = false;
        // check for excessive run time every 10 iterations
        if (++iter_count % 10 == 0 && get_cpu_usage() > start_time + MAX_RUNTIME)
        {
             // This is taking too long. Give up.
             schedulable = false;
             break;
        }
        for (unsigned int k = 0; k < ts.get_task_count() && schedulable; k++)
            if (all_pts[k].get_next(ilen))
            {
                schedulable = is_task_schedulable(k, ts, ilen, i1, sum,
                                                  idiff, ptr);
                point_in_range = true;
            }
    }


    delete[] all_pts;
    delete[] max_test_point;
    delete[] idiff;
    delete[] ptr;

    return schedulable;
}

