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

GlobalNoProgressMechanismLP::GlobalNoProgressMechanismLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: GlobalSuspensionAwareLP(info, task_index, number_of_cpus)
{
	// Constraint 14
	add_no_progress_constraints();

	// Constraint 15 in the paper is the same as constraint 20
	add_no_progress_no_stalling_interference();
}

// The maximum resource-holding H_x,q in the absence of
// a progress mechanism according to Lemma 10 in the paper
unsigned long GlobalNoProgressMechanismLP::resource_hold_time(
		unsigned int tx_id,
		unsigned int res_id)
{
	unsigned int res_exe_time = 0;

	const TaskInfo &tx = taskset[tx_id];

	res_exe_time = tx.get_request_length(res_id);

	if (!res_exe_time)
		return 0;

	unsigned long max_hold = res_exe_time;

	if (tx_id < m)
		return max_hold;

	unsigned long interval;
	do
	{
		// last bound
		interval = max_hold;

		// Bail out if it doesn't converge.
		if (max_hold > tx.get_deadline())
			return UNLIMITED;

		double interf = 0;

		foreach_higher_priority_task_except(taskset, tx, ti, ta)
			interf += ta->workload_bound(interval);

		max_hold = res_exe_time + divide_with_ceil(interf, m);

		// Loop until it converges.
	} while ( interval != max_hold );

	return max_hold;
}

// Constraint 14: no indirect blocking, preemption blocking,
// or co-boosting interference when no progress mechanism is used
void GlobalNoProgressMechanismLP::add_no_progress_constraints()
{
	LinearExpression *exp = new LinearExpression();

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		exp->add_var(vars.co_boosting_interference(tx_id));

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				exp->add_var(vars.indirect(tx_id, q, v));
				exp->add_var(vars.preemption(tx_id, q, v));
			}
		}
	}
	add_inequality(exp, 0);
}

// Constraint 15: rule out stalling interference of each lower-base-priority
// task Tx if all of Tx's lower-base-priority tasks do not access any resource
// requested by Ti
void GlobalNoProgressMechanismLP::add_no_progress_no_stalling_interference()
{
	// find highest-priority task Th with priority less than Ti that
	// accesses a resources used by Ti, but no lower-priority task does.
	unsigned int h;

	for (h = taskset.size() - 1; h > i; h--)
	{
		// check task of priority h - 1
		const TaskInfo& th = taskset[h];

		bool overlap = false;
		foreach(th.get_requests(), req)
		{
			unsigned int q = req->get_resource_id();
			if (ti.get_num_requests(q) > 0)
			{
				overlap = true;
				break;
			}
		}
		if (overlap)
			break;
	}

	LinearExpression *exp = new LinearExpression();

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		// each task with id > index does not access any resource
		// that will be requested by T_i
		if (tx_id >= h)
			exp->add_var(vars.stalling_interference(tx->get_id()));
	}

	add_inequality(exp, 0);
}
