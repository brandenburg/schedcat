#include <algorithm> // for min

#include "tasks.h"
#include "schedulability.h"

#include "edf/bcl.h"

using namespace std;

unsigned long BCLGedf::max_jobs_contained(const Task &t_i, const Task &t_k)
{
    if (t_i.get_deadline() > t_k.get_deadline())
        return 0;
    else
        return 1 + (t_k.get_deadline() - t_i.get_deadline()) / t_i.get_period();
}

void BCLGedf::beta(const Task &t_i, const Task &t_k, fractional_t &beta_i)
{
    unsigned long n = max_jobs_contained(t_i, t_k);

    integral_t c_i, tmp;

    c_i  = t_i.get_wcet();
    tmp  = t_i.get_period();
    tmp *= n;
    if (tmp < t_k.get_deadline())
        // no risk of overflow
        tmp = t_k.get_deadline() - n * t_i.get_period();
    else
        // test says zero is lower limit
        tmp = 0;

    beta_i  = n * c_i;
    beta_i += min(c_i, tmp);
    beta_i /= t_k.get_deadline();
}

bool BCLGedf::is_task_schedulable(unsigned int k, const TaskSet &ts)
{
    fractional_t beta_i, beta_sum = 0;
    fractional_t lambda_term;
    bool small_beta_exists = false;

    ts[k].get_density(lambda_term);
    lambda_term *= -1;
    lambda_term +=  1;

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        if (i != k) {
            beta(ts[i], ts[k], beta_i);
            beta_sum += min(beta_i, lambda_term);
            small_beta_exists = small_beta_exists ||
		    (0 < beta_i && beta_i <= lambda_term);
        }
    }

    lambda_term *= m;

    return beta_sum < lambda_term ||
        (small_beta_exists && beta_sum == lambda_term);
}

bool BCLGedf::is_schedulable(const TaskSet &ts,
                             bool check_preconditions)
{
    if (check_preconditions)
	{
	 if (!(ts.has_only_feasible_tasks() &&
	       ts.is_not_overutilized(m) &&
           ts.has_only_constrained_deadlines() &&
           ts.has_no_self_suspending_tasks()))
	     return false;
    }

    for (unsigned int k = 0; k < ts.get_task_count(); k++)
        if (!is_task_schedulable(k, ts))
            return false;

    return true;
}

