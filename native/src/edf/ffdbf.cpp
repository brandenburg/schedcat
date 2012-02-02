#include <algorithm> // for min
#include <queue>
#include <vector>

#include "tasks.h"
#include "schedulability.h"
#include "math-helper.h"

#include "edf/ffdbf.h"

#include <iostream>
#include "task_io.h"

using namespace std;

static void get_q_r(const Task &t_i, const mpq_class &time,
                    mpz_class &q_i, mpq_class &r_i)
{
    // compute q_i -- floor(time / period)
    //         r_i -- time % period

    r_i = time / t_i.get_period();
    q_i = r_i; // truncate, i.e. implicit floor

    r_i  = time;
    r_i -= q_i * t_i.get_period();
}

static void compute_q_r(const TaskSet &ts, const mpq_class &time,
                        mpz_class q[], mpq_class r[])
{
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        get_q_r(ts[i], time, q[i], r[i]);
}

static void ffdbf(const Task &t_i,
                  const mpq_class &time, const mpq_class &speed,
                  const mpz_class &q_i, const mpq_class &r_i,
                  mpq_class &demand,
                  mpq_class &tmp)
{
    /* this is the cost in all three cases */
    demand += q_i * t_i.get_wcet();

    /* check for (a) and (b) cases */
    tmp  = 0;
    tmp -= t_i.get_wcet();
    tmp /= speed;
    tmp += t_i.get_deadline();
    if (r_i >= tmp)
    {
        // add one more cost charge
        demand += t_i.get_wcet();

        if (r_i <= t_i.get_deadline())
        {
            /* (b) class */
            tmp  = t_i.get_deadline();
            tmp -= r_i;
            tmp *= speed;
            demand -= tmp;
        }
    }
}

static void ffdbf_ts(const TaskSet &ts,
                     const mpz_class q[], const mpq_class r[],
                     const mpq_class &time, const mpq_class &speed,
                     mpq_class &demand, mpq_class &tmp)
{
    demand = 0;
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        ffdbf(ts[i], time, speed, q[i], r[i], demand, tmp);
}


class TestPoints
{
private:
    mpq_class     time;
    mpq_class     with_offset;
    unsigned long period;
    bool          first_point;

public:
    void init(const Task& t_i,
              const mpq_class& speed,
              const mpq_class& min_time)
    {
        period = t_i.get_period();
        with_offset = t_i.get_wcet() / speed;
        if (with_offset > t_i.get_deadline())
            with_offset = t_i.get_deadline();
        with_offset *= -1;

        time = min_time;
        time /= period;
        // round down, i.e., floor()
        mpq_truncate(time);
        time *= period;
        time += t_i.get_deadline();

        with_offset += time;
        first_point = true;

        while (get_cur() <= min_time)
            next();
    }

    const mpq_class& get_cur() const
    {
        if (first_point)
            return with_offset;
        else
            return time;
    }

    void next()
    {
        if (first_point)
            first_point = false;
        else
        {
            time        += period;
            with_offset += period;
            first_point  = true;
        }
    }
};

class TimeComparator {
public:
    bool operator() (TestPoints *a, TestPoints *b)
    {
        return b->get_cur() < a->get_cur();
    }
};

typedef priority_queue<TestPoints*,
                       vector<TestPoints*>,
                       TimeComparator> TimeQueue;

class AllTestPoints
{
private:
    TestPoints *pts;
    TimeQueue   queue;
    mpq_class   last;
    TaskSet const &ts;

public:
    AllTestPoints(const TaskSet &ts)
        : ts(ts)
    {
        pts = new TestPoints[ts.get_task_count()];
    }

    void init(const mpq_class &speed,
              const mpq_class &min_time)
    {
        last = -1;
        // clean out queue
        while (!queue.empty())
            queue.pop();
        // add all iterators
        for (unsigned int i = 0; i < ts.get_task_count(); i++)
        {
            pts[i].init(ts[i], speed, min_time);
            queue.push(pts + i);
        }
    }

    ~AllTestPoints()
    {
        delete[] pts;
    }

    void get_next(mpq_class &t)
    {
        TestPoints* pt;
        do // avoid duplicates
        {
            pt = queue.top();
            queue.pop();
            t = pt->get_cur();
            pt->next();
            queue.push(pt);
        } while (t == last);
        last = t;
    }
};

bool FFDBFGedf::witness_condition(const TaskSet &ts,
                                  const mpz_class q[], const mpq_class r[],
                                  const mpq_class &time,
                                  const mpq_class &speed)
{
    mpq_class demand, bound;

    ffdbf_ts(ts, q, r, time, speed, demand, bound);

    bound  = - ((int) (m - 1));
    bound *= speed;
    bound += m;
    bound *= time;

    return demand <= bound;
}

bool FFDBFGedf::is_schedulable(const TaskSet &ts,
                               bool check_preconditions)
{
    if (m < 2)
        return false;

    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks() &&
              ts.is_not_overutilized(m) &&
              ts.has_only_constrained_deadlines()))
            return false;
    }

    // allocate helpers
    AllTestPoints testing_set(ts);
    mpz_class *q = new mpz_class[ts.get_task_count()];
    mpq_class *r = new mpq_class[ts.get_task_count()];

    mpq_class sigma_bound;
    mpq_class time_bound;
    mpq_class tmp(1, epsilon_denom);

    // compute sigma bound
    tmp = 1;
    tmp /= epsilon_denom;
    ts.get_utilization(sigma_bound);
    sigma_bound -= m;
    sigma_bound /= - ((int) (m - 1)); // neg. to flip sign
    sigma_bound -= tmp; // epsilon
    sigma_bound = min(sigma_bound, mpq_class(1));

    // compute time bound
    time_bound = 0;
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        time_bound += ts[i].get_wcet();
    time_bound /= tmp; // epsilon

    mpq_class t_cur;
    mpq_class sigma_cur, sigma_nxt;
    bool schedulable;

    t_cur = 0;
    schedulable = false;

    // Start with minimum possible sigma value, then try
    // multiples of sigma_step.
    ts.get_max_density(sigma_cur);

    // setup brute force sigma value range
    sigma_nxt = sigma_cur / sigma_step;
    mpq_truncate(sigma_nxt);
    sigma_nxt += 1;
    sigma_nxt *= sigma_step;

    while (!schedulable &&
           sigma_cur <= sigma_bound &&
           t_cur <= time_bound)
    {
        testing_set.init(sigma_cur, t_cur);
        do {
            testing_set.get_next(t_cur);
            if (t_cur <= time_bound)
            {
                compute_q_r(ts, t_cur, q, r);
                schedulable = witness_condition(ts, q, r, t_cur, sigma_cur);
            }
            else
                // exceeded testing interval
                schedulable = true;
        } while (t_cur <= time_bound && schedulable);

        if (!schedulable && t_cur <= time_bound)
        {
            // find next sigma variable
            do
            {
                sigma_cur = sigma_nxt;
                sigma_nxt += sigma_step;
            } while (sigma_cur <= sigma_bound &&
                     !witness_condition(ts, q, r, t_cur, sigma_cur));
        }
    }

    delete [] q;
    delete [] r;

    return schedulable;
}
