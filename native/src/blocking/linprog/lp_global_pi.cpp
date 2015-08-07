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

GlobalPrioInheritanceLP::GlobalPrioInheritanceLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: GlobalSuspensionAwareLP(info, task_index, number_of_cpus),
	  prio_ceilings(get_priority_ceilings(info))
{
	// Constraint 6
	add_pi_no_co_boosting_interference();
	// Constraint 7
	add_pi_m_highest_constraint();
}

// The maximum resource-holding H_x,q under PI
// according to Lemma 8 in the paper
unsigned long GlobalPrioInheritanceLP::resource_hold_time(
		unsigned int tx_id,
		unsigned int res_id)
{
	unsigned int res_exe_time = 0, lower_prio_tx_ti = 0, higher_prio_tx_ti = 0;

	const TaskInfo &tx = taskset[tx_id];
	assert(tx_id == tx.get_id());

	res_exe_time = tx.get_request_length(res_id);

	if (!res_exe_time)
		return 0;

	unsigned long max_hold = res_exe_time;

	if (tx_id < m)
		return max_hold;

	if (tx_id > i)
	{
		lower_prio_tx_ti = tx_id;
		higher_prio_tx_ti = i;
	}
	else
	{
		lower_prio_tx_ti = i;
		higher_prio_tx_ti = tx_id;
	}

	unsigned long interval;
	do
	{
		// last bound
		interval = max_hold;

		// Bail out if it doesn't converge.
		if (max_hold > tx.get_deadline())
			return UNLIMITED;

		unsigned long interf = 0;

		foreach(taskset, ta)
		{
			unsigned int njobs = ta->get_max_num_jobs(interval);

			//interfering workload from higher-priority tasks
			if (ta->get_id() < higher_prio_tx_ti)
				interf += ta->workload_bound(interval);

			//interfering workload from lower-priority tasks
			if ((ta->get_id() > higher_prio_tx_ti)
			    && (ta->get_id() != lower_prio_tx_ti))
			{
				foreach(ta->get_requests(), request)
				{
					unsigned int resource_id = request->get_resource_id();

					//check whether the requested resource has a
					//higher priority ceiling than the priority of
					//the higher priority of Ti and Tx
					if(prio_ceilings[resource_id] < higher_prio_tx_ti)
					{
						unsigned int req_num = ta->get_num_requests(resource_id);
						interf += njobs * req_num * request->get_request_length();
					}
				}
			}
		}

		max_hold = res_exe_time + divide_with_ceil(interf, m);

		// Loop until it converges.
	} while (interval != max_hold);

	return max_hold;
}

// Constraint 6: no co-boosting interference under PI
void GlobalPrioInheritanceLP::add_pi_no_co_boosting_interference()
{
	LinearExpression *exp = new LinearExpression();

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		exp->add_var(vars.co_boosting_interference(tx_id));
	}

	add_inequality(exp, 0);
}

// Constraint 7: the m highest priority tasks will not incur
// any indirect blocking, preemption blocking, or any types of interference
void GlobalPrioInheritanceLP::add_pi_m_highest_constraint()
{
	// for the m highest base priority tasks
	if (ti.get_id() < m)
	{
		LinearExpression *exp = new LinearExpression();

		foreach_task_except(taskset, ti, tx)
		{
			const unsigned int tx_id = tx->get_id();

			if (tx_id < ti.get_id())
				exp->add_var(vars.regular_interference(tx_id));

			if (tx_id > ti.get_id())
			{
				exp->add_var(vars.co_boosting_interference(tx_id));
				exp->add_var(vars.stalling_interference(tx_id));

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
		}

		add_inequality(exp, 0);
	}
}

//-------------------------------
//-------------------------------
// Protocol-specific constraints:
//-------------------------------
//-------------------------------

// Constraint 11: no stalling interference under the PIP and FMLP
void GlobalPrioInheritanceLP::add_pip_fmlp_no_stalling_interference()
{
	LinearExpression *exp = new LinearExpression();

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		exp->add_var(vars.stalling_interference(tx_id));
	}

	add_inequality(exp, 0);
}


// Constraint 12: for each resource lq, the sum of indirect and preemption
//pi-blocking (due to all lower-base-priority tasks) Ji incurred is bounded by
//the number of requests that higher-priority tasks can issue to this resource
//under the PIP and PPCP.
void GlobalPrioInheritanceLP::add_pip_ppcp_indirect_preemption_constraints()
{
	foreach(all_resources, resource)
	{
		unsigned int request_count = 0;

		//the cumulative number of requests for this resource
		//issued by all higher-priority tasks
		foreach_higher_priority_task(taskset, ti, th)
			foreach_request_for(th->get_requests(), *resource, request)
				request_count += request->get_max_num_requests(ti.get_response());

		LinearExpression *exp = new LinearExpression();

		foreach_lower_priority_task(taskset, ti, tx)
		{
			const unsigned int x = tx->get_id();
			const unsigned int q = *resource;

			foreach_request_for(tx->get_requests(), q, request)
			{
				foreach_request_instance(*request, ti, v)
				{
					exp->add_var(vars.indirect(x, q, v));
					exp->add_var(vars.preemption(x, q, v));
				}
			}
		}

		add_inequality(exp, request_count);
	}
}
