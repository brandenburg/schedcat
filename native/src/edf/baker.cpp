#include <algorithm> // for min

#include "tasks.h"
#include "schedulability.h"

#include "edf/baker.h"

using namespace std;

void BakerGedf::beta(const Task &t_i, const Task &t_k,
                     const fractional_t &lambda_k,
                     fractional_t &beta_i)
{
    fractional_t u_i;

    // XXX: possible improvement would be to pre-compute u_i
    //      instead of incurring quadratic u_i computations.
    t_i.get_utilization(u_i);

    beta_i  = t_i.get_period() - t_i.get_deadline();
    beta_i /= t_k.get_deadline();
    beta_i += 1;
    beta_i *= u_i;

    if (lambda_k < u_i)
    {
        fractional_t tmp = t_i.get_wcet();
        tmp -= lambda_k * t_i.get_period();
        tmp /= t_k.get_deadline();
        beta_i += tmp;
    }
}

bool BakerGedf::is_task_schedulable(unsigned int k, const TaskSet &ts)
{
    fractional_t lambda, bound, beta_i, beta_sum = 0;
    fractional_t one = 1;

    ts[k].get_density(lambda);

    bound = m * (1 - lambda) + lambda;

    for (unsigned int i = 0; i < ts.get_task_count() && beta_sum <= bound; i++)
    {
        beta(ts[i], ts[k], lambda, beta_i);
        beta_sum += min(beta_i, one);
    }

    return beta_sum <= bound;
}

bool BakerGedf::is_schedulable(const TaskSet &ts,
                               bool check_preconditions)
{
    if (check_preconditions)
	{
	 if (!(ts.has_only_feasible_tasks() &&
	       ts.is_not_overutilized(m) &&
	       ts.has_no_self_suspending_tasks()))
	     return false;
    }

    for (unsigned int k = 0; k < ts.get_task_count(); k++)
        if (!is_task_schedulable(k, ts))
            return false;

    return true;
}

