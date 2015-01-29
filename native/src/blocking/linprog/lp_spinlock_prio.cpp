#include "lp_common.h"
#include "math-helper.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>


bool add_prio_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

unsigned long get_max_lp_csl(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long max_csl = 0;
	foreach(info.get_tasks(), task)
	{
		foreach(task->get_requests(), request)
		{
			if (request->get_resource_id() == res_id &&
				request->get_request_priority() > locking_prio &&
				ti.get_cluster() != task->get_cluster())
				max_csl = std::max(max_csl, (unsigned long) request->get_request_length());
		}
	}
	return max_csl;
}

unsigned int get_max_reqs(
		const TaskInfo& ti,
		unsigned int res_id)
{
	unsigned int reqs = 0;
	foreach(ti.get_requests(), request)
	{
		if (request->get_resource_id() == res_id)
			reqs += request->get_num_requests();
	}
	return reqs;
}



// computes the term LP^lh in the wait-time bound
unsigned long get_LPlh(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned long W, //current estimate
		std::set<unsigned int>& Qlh)
{
	unsigned long LPlh = 0;

	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster() &&
			task->get_priority() < ti.get_priority())
		{
			foreach(ti.get_requests(), request)
			{
				if (Qlh.find(request->get_resource_id()) != Qlh.end()
					&& request->get_resource_id() != res_id)

					LPlh += divide_with_ceil(W, task->get_period())
							* request->get_num_requests()
							* get_max_lp_csl(info, ti, res_id, request->get_request_priority());
			}
		}
	}
	return LPlh;
}

unsigned long get_cpp(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long cpp_i = 0, cpp_lh = 0;

	// compute cpp^i
	cpp_i = get_max_lp_csl(info, ti, res_id, locking_prio);

	// compute cpp^lh
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster() &&
			task->get_priority() < ti.get_priority()) //local higher-priority task
		{
			foreach(task->get_requests(), request)
			{
				cpp_lh = std::max(cpp_lh,
								  get_max_lp_csl(info, *task,
										         request->get_resource_id(),
										         request->get_request_priority()));
			}
		}
	}

	return std::max(cpp_i, cpp_lh);
}

long bound_wait_time_prio(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio,
		bool preemptive)
{
	unsigned int pi_i_q = get_min_prio(ti, res_id); // minimum priority of any req. of Ti for res_id
	unsigned long wait_time = 0;
	unsigned long LP_Ti = 0;
	std::set<unsigned int> Qlh;
	unsigned long max_CSL_in_Qlh = 0;
	if (preemptive) // consider req.s for all res. accessed by local higher-prio tasks
		Qlh = get_localHP_resources(info, ti);

	// choose initial value for wait_time bound: hit by each job with higher locking prio
	// and once by a single lower locking prio request
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() != ti.get_cluster())
		{
			foreach(task->get_requests(), request)
			{
				if (Qlh.find(request->get_resource_id()) != Qlh.end())
				{
					if (request->get_request_priority() > locking_prio)
					{
						// this is a lower-prio request to a resource in Qlh u {lq}
						// later used to compute LP^P
						// ==> update the max. low-prio CSL
						max_CSL_in_Qlh = std::max(max_CSL_in_Qlh, (unsigned long) request->get_request_length());
					}
					else
					{
						// request->get_request_priority() =< locking_prio
						// this is a higher-prio request; add total CSL to initial wait time
						// corresponds to the S(l_q,pi) term in the wait-time recurrence
						wait_time += request->get_request_length() * request->get_num_requests();
					}
				}

				if (request->get_resource_id() == res_id &&
					request->get_request_priority() > pi_i_q)
				{
					// this is a lower-prio request to the resource Ti accesses, lq
					// ==> keep track of the max. CSL
					LP_Ti = std::max(LP_Ti, (unsigned long) request->get_request_length());
				}
			}
		}
	}

	unsigned long delay_by_lower = 0;

	// Ti's request can be directly delayed once by a remote lower-prio request
	// ==> account for the longest remote CSL for the resource Ti accesses (LP_Ti)
	delay_by_lower += LP_Ti;

	if (preemptive)
	{
		// interference through execution of local higher-priority tasks
		// this corresponds to the I(l_q,pi) term in the wait-time recurrence
		wait_time += get_hp_interference(info, ti, ti.get_response());

		// factor in the number of preemptions to determine the actual delay by lower-prio req.s
		// this corresponds to the LP^P(l_q,pi) term in the wait-time recurrence
		delay_by_lower += get_cpp(info, ti, res_id, locking_prio) * max_preemptions(info, ti, wait_time);

		// corresponds to LP^lh - delay of local higher-prio tasks due to remote lower-prio req.s
		wait_time += get_LPlh(info, ti, res_id, locking_prio, wait_time, Qlh);
	}

	wait_time += delay_by_lower;

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
				// if current resource is l_q or is in Qlh and
				// task is remote
				if ((Qlh.find(request->get_resource_id()) != Qlh.end() ||
					request->get_resource_id() == res_id) &&
					task->get_cluster() != ti.get_cluster())
				{
					// if request has higher-equal priority
					if (request->get_request_priority() <= locking_prio)
					{
						// this is a higher-prio request; add to total CSL
						// corresponds to the S(l_q,pi) term in the wait-time recurrence
						delay_by_higher += request->get_request_length()
										   * request->get_max_num_requests(estimate);
					}
				}
			}
		}
		// account for LP^Ti: blocking of Ti's request by remote lower-prio req.s
		delay_by_lower = LP_Ti;
		if (preemptive)
		{
			// for each preemption: account for max. CSL of Ti or local higher-prio jobs
			delay_by_lower += get_cpp(info, ti, res_id, locking_prio) * max_preemptions(info, ti, estimate);

			// corresponds to LP^lh - delay of local higher-prio tasks due to remote lower-prio req.s
			delay_by_lower += get_LPlh(info, ti, res_id, locking_prio, estimate, Qlh);

			// corresponds to I(l_q,pi) - interference due to local higher-prio tasks
			delay_by_higher += get_hp_interference(info, ti, estimate);
		}
		new_estimate = delay_by_lower + delay_by_higher;

		// add one epsilon to wait-time bound to ensure that Ti's request finally succeeds
		new_estimate += 1;
	}
	if (estimate <= ti.get_period())
		return estimate; // wait time converged; return wait time
	else
		return -1; // wait time didn't converge; return error value
}

// Constraint 12: Limit spin-delay due to higher-priority requests
// using the wait-time bound.
// Constraint 25: ~ for preemptive
bool add_prio_direct_blocking_HP_constraints(
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
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks
		long wait_time_bound = bound_wait_time_prio(info, ti, *resource, min_prioHP, preemptive);

		if (wait_time_bound < 0) // fail if wait-time cannot be bounded
			wait_time_bound = ti.get_response();

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (ti.get_cluster() == c) // ignore Ti's cluster
				continue;

			foreach(clusters[c], task)
			{
				LinearExpression *exp = new LinearExpression();
				unsigned int max_num_reqs = 0;
				foreach((*task)->get_requests(), request)
				{
					if ((*request).get_resource_id() == *resource &&
						(*request).get_request_priority() <= min_prioHP)
					{
						max_num_reqs += request->get_max_num_requests(wait_time_bound);
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_DIRECT);
							exp->add_var(var_id);
						}
					}
				}

				if (exp->has_terms())
					lp.add_inequality(exp, max_num_reqs * ncs);
				else
					delete exp;
			}
		}
	}
	return true;
}

// Constraint 13: Limit spin-delay due to lower-priority requests
// to at most once for each request for lq issued by Ti or a local
// higher-priority task.
// Constraint 14: Limit arrival-blocking due to lower-priority requests
// to at most once for each resource, if arrival-blocking is allowed at
// all according to Aq.
// Constraint 25: related to Constraint 13, for preemptive spinlocks
void add_prio_blocking_LP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive)
{
	std::set<unsigned int> all_resources = get_all_resources(info);

	foreach(all_resources, resource)
	{
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks

		LinearExpression *exp_direct = new LinearExpression();
		LinearExpression *exp_arrival = NULL;
		if (!preemptive)
			exp_arrival = new LinearExpression();

		foreach(info.get_tasks(), task)
		{
			if (ti.get_cluster() != (*task).get_cluster()) // ignore Ti's cluster
			{
				foreach((*task).get_requests(), request)
				{
					if ((*request).get_resource_id() == *resource &&
						(*request).get_request_priority() > min_prioHP)
					{
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task).get_id(), (*request).get_resource_id(), v, BLOCKING_DIRECT);
							exp_direct->add_var(var_id); // Constraint 13/25
							if (!preemptive)
							{
								var_id = vars.lookup((*task).get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
								exp_arrival->add_var(var_id); //Constraint 14
							}
						}
					}
				}
			}
		}

		if (exp_direct->has_terms())
		{
			if (preemptive)
			{
				unsigned int var_id = vars.lookup_max_preemptions(*resource);
				exp_direct->sub_var(var_id); // Constraint 25
			}
			lp.add_inequality(exp_direct, ncs); // Constraint 13/25
			if (!preemptive)
			{
				assert(exp_arrival->has_terms());
				unsigned int var_id = vars.lookup_arrival_enabled(*resource);
				exp_arrival->sub_var(var_id); //Constraint 14
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

// Constraint 15:  Limit arrival-blocking due to higher-priority requests
// using the wait-time bound.
bool add_prio_arrival_blocking_HP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		unsigned int min_prioLP = get_min_prio(info, ti, *resource, true);
		long wait_time_bound = bound_wait_time_prio(info, ti, *resource, min_prioLP, false);

		if (wait_time_bound < 0) // fail if wait-time cannot be bounded
			wait_time_bound = ti.get_response();

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (ti.get_cluster() == c) // ignore Ti's cluster
				continue;

			foreach(clusters[c], task)
			{
				LinearExpression *exp = new LinearExpression();
				unsigned int max_num_reqs = 0;
				foreach((*task)->get_requests(), request)
				{
					if ((*request).get_resource_id() == *resource &&
						(*request).get_request_priority() <= min_prioLP)
					{
						max_num_reqs += request->get_max_num_requests(wait_time_bound);
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
							exp->add_var(var_id);
						}
					}
				}

				if (exp->has_terms())
				{
					unsigned int var_id = vars.lookup_arrival_enabled(*resource);
					exp->sub_term(max_num_reqs, var_id);
					lp.add_inequality(exp, 0);
				}
				else
					delete exp;
			}
		}
	}
	return true;
}

bool add_prio_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool preemptive)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	if (preemptive)
		add_common_preemptive_spinlock_constraints(vars, info, ti, lp);

	// Constraint 12
	add_prio_direct_blocking_HP_constraints(vars, info, ti, lp, preemptive);

	// Constraint 13, Constraint 14, Constraint 25
	add_prio_blocking_LP_constraints(vars, info, ti, lp, preemptive);

	// Constraint 15
	if (!preemptive)
		add_prio_arrival_blocking_HP_constraints(vars, info, ti, lp);

	return true;
}


bool apply_prio_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	bool preemptive)
{
	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	add_prio_constraints(vars, info, ti, lp, preemptive);

	set_spinlock_blocking_objective(vars, info, ti, lp);
	vars.seal();
	Solution *sol = linprog_solve(lp, vars.get_num_vars());

	assert(sol != NULL);
	Interference total;
	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;

	delete sol;
	return true;
}

BlockingBounds* lp_pfp_prio_spinlock_bounds(const ResourceSharingInfo& info, bool preemptive)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_prio_bounds_for_task(i, *results, info, preemptive);
	}

	return results;
}
