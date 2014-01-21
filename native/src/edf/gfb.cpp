#include "tasks.h"
#include "schedulability.h"

#include "edf/gfb.h"

bool GFBGedf::is_schedulable(const TaskSet &ts, bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks()
              && ts.is_not_overutilized(m)
              && ts.has_only_constrained_deadlines()
              && ts.has_no_self_suspending_tasks()))
            return false;
    }

    fractional_t total_density, max_density, bound;

    ts.get_density(total_density);
    ts.get_max_density(max_density);

    bound = m - (m - 1) * max_density;

    return total_density <= bound;
}
