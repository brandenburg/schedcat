#include "lp_common.h"
#include "cpu_time.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>


long bound_wait_time(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		bool preemptive)
{
	unsigned long wait_time = 0;
	std::set<unsigned int> Qlh;
	if (preemptive) // consider req.s for all res. accessed by local higher-prio tasks
		Qlh = get_localHP_resources(info, ti);
	else // otherwise just consider the resource we're accessing here
		Qlh.insert(res_id);

	// choose initial value for wait_time bound: hit by each job with higher locking prio
	// and once by a single lower locking prio request
	foreach(info.get_tasks(), task)
	{
		foreach(task->get_requests(), request)
		{
			if (Qlh.find(request->get_resource_id()) != Qlh.end() &&
				task->get_cluster() != ti.get_cluster())
			{
				// add total CSL to initial wait time
				wait_time += request->get_request_length() * request->get_num_requests();
			}
		}
	}

	if (preemptive)
		wait_time += get_hp_interference(info, ti, ti.get_response());

	unsigned long estimate = 0, new_estimate = wait_time;

	// use RTA to find max. wait time
	while (estimate <= ti.get_period() && estimate != new_estimate)
	{
		unsigned long delay_by_higher = 0;
		estimate = new_estimate;

		foreach(info.get_tasks(), task)
		{
			foreach(task->get_requests(), request)
			{
				if ((Qlh.find(request->get_resource_id()) != Qlh.end() ||
					request->get_resource_id() == res_id) &&
					task->get_cluster() != ti.get_cluster())
					// add to total CSL
					delay_by_higher += request->get_request_length() * request->get_max_num_requests(estimate);
			}
		}
		if (preemptive)
			delay_by_higher += get_hp_interference(info, ti, estimate);
		new_estimate = delay_by_higher;

		// add one epsilon to wait-time bound to ensure that Ti's request finally succeeds
		new_estimate += 1;
	}

	if (estimate <= ti.get_period())
		return estimate; // wait time converged; return wait time
	else
		return -1; // wait time didn't converge; return error value
}

// Constraints 10/11: unordered spinlocks
bool add_unordered_direct_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks
		long wait_time_bound = bound_wait_time(info, ti, *resource, preemptive);

		if (wait_time_bound < 0) // use response time if wait-time cannot be bounded
			wait_time_bound = ti.get_response();

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (ti.get_cluster() == c) // ignore Ti's cluster
				continue;

			foreach(clusters[c], task)
			{
				LinearExpression *exp_direct = new LinearExpression();
				LinearExpression *exp_arrival = NULL;
				if (!preemptive)
					exp_arrival = new LinearExpression();
				unsigned int max_num_reqs = 0;
				foreach((*task)->get_requests(), request)
				{
					if ((*request).get_resource_id() == *resource)
					{
						max_num_reqs += request->get_max_num_requests(wait_time_bound);
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_DIRECT);
							exp_direct->add_var(var_id);
							if (!preemptive)
							{
								var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
								exp_arrival->add_var(var_id);
							}
						}
					}
				}

				if (exp_direct->has_terms())
				{
					lp.add_inequality(exp_direct, max_num_reqs * ncs);
					if (!preemptive)
					{
						assert(exp_arrival->has_terms());
						unsigned int var_id = vars.lookup_arrival_enabled(*resource);
						exp_arrival->sub_term(max_num_reqs, var_id);
						lp.add_inequality(exp_arrival, 0);
					}
				}
				else
				{
					delete exp_direct;
					if (!preemptive)
					{
						assert(!exp_arrival->has_terms());
						delete exp_arrival;
					}
				}
			}
		}
	}
	return true;
}

void add_unordered_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool preemptive)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	if (preemptive)
		add_common_preemptive_spinlock_constraints(vars, info, ti, lp);

	// Constraints 10/11
	add_unordered_direct_blocking_constraints(vars, info, ti, lp, preemptive);
}

bool apply_unordered_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	bool preemptive)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(unordered_bounds);
	unordered_bounds.start();
#endif

	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	add_unordered_constraints(vars, info, ti, lp, preemptive);
	set_spinlock_blocking_objective(vars, info, ti, lp);
	vars.seal();
	Solution *sol = linprog_solve(lp, vars.get_num_vars());

	assert(sol != NULL);
	Interference total;
	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;

	delete sol;
#if DEBUG_LP_OVERHEADS >= 1
	unordered_bounds.stop();
	std::cout << unordered_bounds << std::endl;
#endif

	return true;
}


BlockingBounds* lp_pfp_unordered_spinlock_bounds(const ResourceSharingInfo& info, bool preemptive)
{
	BlockingBounds* results = new BlockingBounds(info);

	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_unordered_bounds_for_task(i, *results, info, preemptive);
	}

	return results;
}
