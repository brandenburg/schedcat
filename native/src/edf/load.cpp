#include "tasks.h"
#include "schedulability.h"

#include "edf/load.h"

#include <iostream>
#include <algorithm>

/* This implements the LOAD test presented in:
 *
 *   Baker & Baruah (2009), An analysis of global EDF schedulability for
 *   arbitrary-deadline sporadic task systems, Real-Time Systems, volume 43,
 *   pages 3-24.
 */

bool LoadGedf::is_schedulable(const TaskSet &ts, bool check_preconditions)
{
    if (check_preconditions)
	{
        if (!(ts.has_only_feasible_tasks()
              && ts.is_not_overutilized(m)
              && ts.has_no_self_suspending_tasks()))
            return false;
    }

    fractional_t load, max_density, mu, bound, cond1, cond2;
    integral_t mu_ceil;

    // get the load of the task set
    ts.approx_load(load, epsilon);

    // compute bound (corollary 2)
    ts.get_max_density(max_density);

    mu = m - (m - 1) * max_density;

    mu_ceil = mu.get_num();
    // divide with ceiling
    mpz_cdiv_q(mu_ceil.get_mpz_t(),
               mu.get_num().get_mpz_t(),
               mu.get_den().get_mpz_t());

    cond1 = mu - (mu_ceil - 1) * max_density;
    cond2 = (mu_ceil - 1) - (mu_ceil - 2) * max_density;

    bound = std::max(cond1, cond2);

    return load <= bound;
}
