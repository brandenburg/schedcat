#include <stdint.h>
#include <cassert>
#include <climits>
#include <cmath>

#include "linprog/varmapperbase.h"
#include "linprog/solver.h"

#include "sharedres_types.h"

#include "iter-helper.h"
#include "stl-helper.h"
#include "stl-io-helper.h"
#include "math-helper.h"

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

#include "lp_global.h"

GlobalPriorityQueuesLP::GlobalPriorityQueuesLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: GlobalSuspensionAwareLP(info, task_index, number_of_cpus)
{
	// Constraint 9
	add_prio_lower_direct_constraints();

}

void GlobalPriorityQueuesLP::add_constraints_post_ctor()
{
	// Constraint 10
	// cannot be called from within constructor
	add_prio_higher_direct_constraints();
}

// Bounding the time Ti spends on waiting
// for a lower-priority task to release
// the resource with id "res_id"
unsigned long GlobalPriorityQueuesLP::wait_lower_prio(unsigned int res_id)
{
	unsigned long res_hold_time = 0, maxh = 0;

	//Find the maximum resource-holding time (for "res_id")
	//among all lower-priority tasks
	foreach_lower_priority_task(taskset, ti, tl)
	{
		// Note: could return UNLIMITED
	        res_hold_time = resource_hold_time(tl->get_id(), res_id);
		maxh = std::max(maxh, res_hold_time);
	}

	return maxh;
}

// Bounding the time Ti spends on waiting
// for higher-priority tasks to release
// the resource with id "res_id"
unsigned long GlobalPriorityQueuesLP::wait_higher_prio(
		unsigned int res_id, unsigned long interval)
{
	unsigned long sum = 0;

	foreach_higher_priority_task(taskset, ti, th)
	{
		foreach_request_for(th->get_requests(), res_id, request)
		{
			unsigned int njobs      = th->get_max_num_jobs(interval);
			unsigned int num_req_th = th->get_num_requests(res_id);
			unsigned long res_hold_time =
				resource_hold_time(th->get_id(), res_id);

			// check for convergence failure
			if (res_hold_time == UNLIMITED)
				return UNLIMITED;

			sum += njobs * num_req_th * res_hold_time;
		}
	}

	return sum;
}

// Bounding the maximum time that Ti spends on
// waiting for the resource with id "res_id"
unsigned long GlobalPriorityQueuesLP::resource_wait_time(
		unsigned int res_id)
{
	// Get the maximum time Ti spends on waiting
	// for a lower-priority task
	const unsigned long wait_lowerprio = wait_lower_prio(res_id);
	unsigned long max_wait = wait_lowerprio + 1, interval;

	// check for convergence failure
	if (wait_lowerprio == UNLIMITED)
		return UNLIMITED;

	do
	{
		// last bound
		interval = max_wait;

		// Bail out if it doesn't converge.
		if (interval > ti.get_deadline())
			return UNLIMITED;

		const unsigned long whp = wait_higher_prio(res_id, interval);
		// check for convergence failure
		if (whp == UNLIMITED)
			return UNLIMITED;

		max_wait = wait_lowerprio + whp + 1;

		// Loop until it converges.
	} while (interval != max_wait);

	return max_wait;
}

// Bounding the number of direct blockings on a resource
// (with resource id equal to "res_id") that Ti incurred
// due to a higher-priority task Tx
unsigned int GlobalPriorityQueuesLP::higher_direc_num_req(
		unsigned int tx_id, unsigned int res_id)
{
	const TaskInfo& tx = taskset[tx_id];

	//Get the maximum waiting time of Ti on the resource
	unsigned long res_wait_time = resource_wait_time(res_id);

	// check for convergence failure
	if (res_wait_time == UNLIMITED)
		return UNLIMITED;

	unsigned int njobs          = tx.get_max_num_jobs(res_wait_time);
	unsigned int num_req_tx     = tx.get_num_requests(res_id);
	unsigned int num_req_ti     = ti.get_num_requests(res_id);

	return njobs * num_req_tx * num_req_ti;
}

// Constraint 9: for each resource lq, at most one request form lower-priority
// tasks directly delays Ji, under priority queuing
void GlobalPriorityQueuesLP::add_prio_lower_direct_constraints()
{
	foreach(all_resources, res_id)
	{
		LinearExpression *exp = new LinearExpression();

		unsigned int num_of_requests = ti.get_num_requests(*res_id);

		foreach_lower_priority_task(taskset, ti, tx)
		{
			const unsigned int x = tx->get_id();
			const unsigned int q = *res_id;

			foreach_request_for(tx->get_requests(), q, request)
			{
				foreach_request_instance(*request, ti, v)
					exp->add_var(vars.direct(x, q, v));
			}
		}

		add_inequality(exp, num_of_requests);
	}
}

// Constraint 10: incorporate response-time analysis to limit direct
// pi-blocking caused by higher-priority tasks, under priority queuing
void GlobalPriorityQueuesLP::add_prio_higher_direct_constraints()
{
	foreach_higher_priority_task(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();

		foreach(all_resources, res_id)
		{
			const unsigned int q = *res_id;
			foreach_request_for(tx->get_requests(), q, request)
			{
				LinearExpression *exp = new LinearExpression();

				foreach_request_instance(*request, ti, v)
					exp->add_var(vars.direct(x, q, v));

				// get the maximum number of times that Ti can be
				// directly pi-blocked by higher-priority tasks
				// NOTE: higher_direc_num_req() can return UNLIMITED

				unsigned int max_num = higher_direc_num_req(x, q);
				if (max_num != UNLIMITED)
					add_inequality(exp, max_num);
				else
					delete exp; // failed to converge
			}
		}
	}
}
