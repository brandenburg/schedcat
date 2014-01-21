#include <algorithm>
#include <assert.h>

#include "tasks.h"
#include "schedulability.h"

#include "edf/bcl_iterative.h"

using namespace std;

static void interfering_workload(const Task &t_i,
                                 const Task &t_k,
                                 unsigned long slack,
                                 integral_t &inf)
{
    unsigned long njobs = t_k.get_deadline() / t_i.get_period();

    inf  = njobs;
    inf *= t_i.get_wcet();

    unsigned long tmp = slack + njobs * t_i.get_period();

    if (t_k.get_deadline() >= tmp)
        inf += min(t_i.get_wcet(), t_k.get_deadline() - tmp);
    //else inf += min(t.get_wcet(), 0) // always null by definition.
}

bool BCLIterativeGedf::slack_update(unsigned int k,
                                    const TaskSet &ts,
                                    unsigned long *slack,
                                    bool &has_slack)
{
    integral_t other_work = 0;
    integral_t inf;
    integral_t inf_bound = ts[k].get_deadline() - ts[k].get_wcet() + 1;

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        if (k != i)
        {
            interfering_workload(ts[i], ts[k], slack[i], inf);
            other_work += min(inf, inf_bound);
        }
    other_work /= m;
    unsigned long tmp = ts[k].get_wcet() + other_work.get_ui();

    assert( other_work.fits_ulong_p() );
    assert (tmp > other_work.get_ui() );

    has_slack = tmp <= ts[k].get_deadline();
    if (!has_slack)
        // negative slack => no update, always assume zero
        return false;
    else
    {
        tmp = ts[k].get_deadline() - tmp;
        if (tmp > slack[k])
        {
            // better slack => update
            slack[k] = tmp;
            return true;
        }
        else
            // no improvement
            return false;
    }
}

bool BCLIterativeGedf::is_schedulable(const TaskSet &ts,
                                      bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks()
              && ts.is_not_overutilized(m)
              && ts.has_only_constrained_deadlines()
              && ts.has_no_self_suspending_tasks()))
            return false;
        if (ts.get_task_count() == 0)
            return true;
    }

    unsigned long* slack = new unsigned long[ts.get_task_count()];

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        slack[i] = 0;

    unsigned long round = 0;
    bool schedulable = false;
    bool updated     = true;

    while (updated && !schedulable && (max_rounds == 0 || round < max_rounds))
    {
        round++;
        schedulable = true;
        updated     = false;
        for (unsigned int k = 0; k < ts.get_task_count(); k++)
        {
            bool ok;
            if (slack_update(k, ts, slack, ok))
                updated = true;
            schedulable = schedulable && ok;
        }
    }

    return schedulable;
}
