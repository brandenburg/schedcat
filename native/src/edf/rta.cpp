#include <algorithm>
#include <assert.h>

#include "tasks.h"
#include "schedulability.h"

#include "edf/rta.h"

#include <iostream>
#include "task_io.h"

using namespace std;


static void rta_interfering_workload(const Task &t_i,
                                     unsigned long response_time,
                                     unsigned long slack_i,
                                     integral_t &inf,
                                     integral_t &interval)
{
    interval = response_time;
    interval += t_i.get_deadline() - t_i.get_wcet();
    interval -= slack_i;

    inf  = t_i.get_wcet();
    inf *= interval / t_i.get_period();

    interval %= t_i.get_period();
    if (interval > t_i.get_wcet())
        inf += t_i.get_wcet();
    else
        inf += interval;
}


static void edf_interfering_workload(const Task &t_i,
                                     const Task &t_k,
                                     unsigned long slack_i,
                                     integral_t &inf)
{
    /* implicit floor in integer division */
    unsigned long njobs = t_k.get_deadline() / t_i.get_period();

    inf  = njobs;
    inf *= t_i.get_wcet();

    unsigned long tmp = t_k.get_deadline() % t_i.get_period();
    if (tmp > slack_i)
        /* if tmp <= slack_i, then zero would be added */
        inf += min(t_i.get_wcet(), tmp - slack_i);
}

bool RTAGedf::response_estimate(unsigned int k,
                                const TaskSet &ts,
                                unsigned long const *slack,
                                unsigned long response,
                                unsigned long &new_response)
{
    integral_t other_work = 0;
    integral_t inf_edf;
    integral_t inf_rta;
    integral_t inf_bound = response - ts[k].get_wcet() + 1;
    integral_t tmp;

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        if (k != i)
        {
            edf_interfering_workload(ts[i], ts[k], slack[i], inf_edf);
            rta_interfering_workload(ts[i], response, slack[i], inf_rta, tmp);
            other_work += min(min(inf_edf, inf_rta), inf_bound);
        }
    /* implicit floor */
    other_work /= m;
    other_work += ts[k].get_wcet();
    if (other_work.fits_ulong_p())
    {
        new_response = other_work.get_ui();
        return true;
    }
    else
    {
        /* overflowed => reponse time > deadline */
        return false;
    }
}

bool RTAGedf::rta_fixpoint(unsigned int k,
                           const TaskSet &ts,
                           unsigned long const *slack,
                           unsigned long &response)
{
    unsigned long last;
    bool ok;

    last = ts[k].get_wcet();
    ok = response_estimate(k, ts, slack, last, response);

    while (ok && last != response && response <= ts[k].get_deadline())
    {
        if (last < response && response  - last < min_delta)
            last = min(last + min_delta, ts[k].get_deadline());
        else
            last = response;
        ok = response_estimate(k, ts, slack, last, response);
    }

    return ok && response <= ts[k].get_deadline();
}

bool RTAGedf::is_schedulable(const TaskSet &ts, bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks()
              && ts.is_not_overutilized(m)
              && ts.has_only_constrained_deadlines()
              && ts.has_only_feasible_tasks()))
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
            unsigned long response, new_slack;
            if (rta_fixpoint(k, ts, slack, response))
            {
                new_slack = ts[k].get_deadline() - response;
                if (new_slack != slack[k])
                {
                    slack[k] = new_slack;
                    updated = true;
                }
            }
            else
            {
                schedulable = false;
            }
        }
    }

    return schedulable;
}
