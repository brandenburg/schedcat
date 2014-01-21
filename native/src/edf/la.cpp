/* Implementation of the s-aware G-EDF schedulability test proposed in
 *   "Suspension-Aware Analysis for Hard Real-Time Multiprocessor Scheduling"
 *   by Cong Liu and James H. Anderson, ECRTS 2013
 */

#include <algorithm> // for greater
#include <queue>
#include <vector>

#include "math-helper.h"
#include "tasks.h"
#include "schedulability.h"

#include "edf/la.h"

#include <iostream>
#include "task_io.h"

#include "cpu_time.h"

using namespace std;

const double LAGedf::MAX_RUNTIME = 1.0; /* second per task per suspension length */

/* To be similar to the BaruahGedf implementation, `interval' is A_k (in Bar:07),
 * which is equivalent to xi_l - d_l in LA:13 */

namespace LA {

    class DBFPointsOfChange
    {
    private:
        integral_t     cur;
        unsigned long pi; // period

    public:
        void init(const Task& tsk_i, const Task& tsk_k)
        {
            unsigned long dk, di;

            dk = tsk_k.get_deadline();
            di = tsk_i.get_deadline();
            pi = tsk_i.get_period();

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

    class AllTestPoints
    {
    private:
        DBFPointsOfChange *dbf;
        DBFQueue queue;
        integral_t last;
        integral_t upper_bound;

    public:
        AllTestPoints(const TaskSet &ts, int k, const integral_t &bound)
            : upper_bound(bound)
        {
            last = -1;
            dbf = new DBFPointsOfChange[ts.get_task_count()];
            for (unsigned int i = 0; i < ts.get_task_count(); i++)
            {
                dbf[i].init(ts[i], ts[k]);
                queue.push(dbf + i);
            }
        }

        ~AllTestPoints()
        {
            delete[] dbf;
        }

        bool get_next(integral_t &t)
        {
            if (last > upper_bound)
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

            return last <= upper_bound;
        }
    };

}

static void work_no_carry(
    unsigned int i,
    unsigned int l,
    const TaskSet &ts,
    const integral_t &ilen,
    integral_t &wnc,
    unsigned long susp
)
{
    integral_t dbf, tmp;
    tmp = ilen + ts[l].get_deadline(); /* tmp = xi_l - lambda_l */
    dbf = ts[i].dbf(tmp);
    if (i == l)
        wnc = min(integral_t(dbf - ts[l].get_wcet()),
                 max(integral_t(tmp - ts[l].get_deadline()),
                     integral_t((tmp + ts[l].get_tardiness_threshold())
                                - ts[l].get_period())));
    else
        wnc = min(dbf,
                 integral_t(tmp + ts[l].get_tardiness_threshold()
                            - ts[l].get_wcet() - susp + 1));
}


static integral_t delta(
    const Task &tsk,
    const integral_t &t)
{
    integral_t period = tsk.get_period();
    integral_t wcet   = tsk.get_wcet();
    integral_t tmp;

    tmp = divide_with_ceil(t, period);

    integral_t db;

    db  = (tmp - 1) * wcet;
    db += min(wcet, integral_t(t - tmp * period + period));

    return db;
}

static void work_carry_in(
    unsigned int i,
    unsigned int l,
    const TaskSet &ts,
    const integral_t &ilen,
    integral_t &wc,
    unsigned long susp)
{
    integral_t dbf, tmp;

    tmp = ilen + ts[l].get_deadline(); /* tmp = xi_l - lambda_l */

    if (i == l) {
        dbf = delta(ts[l], tmp + ts[l].get_tardiness_threshold());
        wc = min(integral_t(dbf - ts[l].get_wcet()),
                 max(integral_t(tmp - ts[l].get_deadline()),
                     integral_t((tmp + ts[l].get_tardiness_threshold())
                                - ts[l].get_period())));
    } else {
        dbf = delta(ts[i], tmp + ts[i].get_tardiness_threshold());
        wc = min(dbf,
                 integral_t(((tmp + ts[l].get_tardiness_threshold()) -
                             ts[l].get_wcet()) - susp + 1));
    }
}

class MPZComparator {
public:
    bool operator() (integral_t *a, integral_t *b)
    {
        return *b < *a;
    }
};

bool LAGedf::is_task_schedulable_for_interval(
    const TaskSet &ts,
	unsigned int l,
	unsigned long suspend,
	const integral_t &ilen, /* interval length is xi_l - d_l */
	integral_t &i1,
	integral_t &sum,
	integral_t *idiff,
	integral_t **ptr)
{
    integral_t bound;
    sum = 0;

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        work_no_carry(i, l, ts, ilen, i1, suspend);
        work_carry_in(i, l, ts, ilen, idiff[i], suspend);

        if (ts[i].is_self_suspending())
        {
            sum      += std::max(i1, idiff[i]);
            idiff[i] = 0;
        }
        else
        {
            sum      += i1;
            idiff[i] -= i1;
        }
    }

    /* sort pointers to idiff to find largest idiff values */
    sort(ptr, ptr + ts.get_task_count(), MPZComparator());

    /* Get m-1 largest idiff values for compute tasks
     * (self-suspending tasks have zero idiff). */
    for (unsigned int i = 0; i < m - 1 && i < ts.get_task_count(); i++)
        sum += *ptr[i];

    bound  = ilen + ts[l].get_deadline() + ts[l].get_tardiness_threshold()
                  - ts[l].get_wcet() - suspend;
    bound *= m;

//    cout << "     ilen=" << ilen << " => sum=" << sum << " <?= bound=" << bound << endl;

    return sum <= bound;
}

integral_t LAGedf::get_max_test_point(
    const TaskSet &ts,
    unsigned int l,
    const fractional_t &m_minus_u,
	const fractional_t &test_point_sum,
	const fractional_t &usum,
    unsigned long suspension)
{
    fractional_t sum = 0;

//     cout << "  XX m=" << m
//         << " m_minus_u=" << m_minus_u
//         << " test_point_sum=" << test_point_sum
//         << " usum=" << usum
//         << " susp=" << suspension
//         << endl;

    sum = m;
    sum *= ts[l].get_wcet() + suspension;
    sum -= usum * ts[l].get_tardiness_threshold();
    sum += test_point_sum;

    sum /= m_minus_u;
    return round_up(sum);
}


bool LAGedf::is_task_schedulable_for_suspension_length(
    const TaskSet &ts,
	unsigned int l,
	unsigned long suspend,
	const fractional_t &m_minus_u,
	const fractional_t &test_point_sum,
	const fractional_t &usum)
{
    bool schedulable = true;

    integral_t *idiff, i1, sum;
    integral_t** ptr; // indirect access to idiff

    idiff          = new integral_t[ts.get_task_count()];
    ptr            = new integral_t*[ts.get_task_count()];
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        ptr[i] = idiff + i;

    LA::AllTestPoints all_pts(ts, l,
        get_max_test_point(ts, l, m_minus_u, test_point_sum, usum, suspend));

//    cout << "    up to " << get_max_test_point(ts, l, m_minus_u, test_point_sum, usum, suspend) << endl;

    unsigned long iter_count = 0;
    double start_time = get_cpu_usage();

    for (integral_t ilen = 0; schedulable && all_pts.get_next(ilen); )
    {
        // check for excessive run time every 10 iterations
        if (++iter_count % 10 == 0 && get_cpu_usage() > start_time + MAX_RUNTIME)
             // This is taking too long. Give up.
            schedulable = false;
        else
            schedulable = is_task_schedulable_for_interval(
                                ts, l, suspend, ilen, i1, sum, idiff, ptr);
    }

    delete [] idiff;
    delete [] ptr;
    return schedulable;
}

bool LAGedf::is_schedulable(const TaskSet &ts,
                                bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks() &&
              ts.is_not_overutilized(m)))
            return false;

        if (ts.get_task_count() == 0)
            return true;
    }

    fractional_t m_minus_u, usum;
    ts.get_utilization(usum);
    m_minus_u = m - usum;

    if (m_minus_u <= 0) {
        // Liu & Anderson's test requires strictly positive slack.
        // In the case of zero slack the testing interval becomes
        // infinite. Therefore, we can't do anything but bail out.
        return false;
    }

    // pre-compute static part of max test point calculation
    fractional_t test_point_sum = 0;
    fractional_t u;
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        test_point_sum += ts[i].get_wcet();
        ts[i].get_utilization(u);
        test_point_sum += u * ts[i].get_tardiness_threshold();
    }


    bool schedulable = true;
    for (unsigned int l = 0; l < ts.get_task_count() && schedulable; l++)
    {
        for (unsigned long suspension = 0;
             suspension <= ts[l].get_self_suspension() && schedulable;
             suspension++)
        {
//            cout << "Testing " << ts[l] << " susp = " << suspension << endl;
            if (!is_task_schedulable_for_suspension_length(ts, l, suspension,
                    m_minus_u, test_point_sum, usum))
                schedulable = false;

        }
    }

    return schedulable;
}

