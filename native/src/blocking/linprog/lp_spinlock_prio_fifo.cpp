#include "lp_common.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>


bool add_prio_fifo_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

long get_max_CS_per_cluster(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	Clusters clusters;
	split_by_cluster(info, clusters);
	unsigned long sum = 0;

	for (unsigned int c=0; c < clusters.size(); c++)
	{
		if (ti.get_cluster() == c) // ignore Ti's cluster
			continue;

		unsigned int longest_CS_on_cluster = 0;
		foreach(clusters[c], task)
		{
			foreach((*task)->get_requests(), request)
			{
				if ((*request).get_resource_id() == res_id &&
					(*request).get_request_priority() == locking_prio)
				{
					longest_CS_on_cluster = std::max(longest_CS_on_cluster, (*request).get_request_length());
				}
			}
		}
		sum += longest_CS_on_cluster;
	}

	return sum;
}

long bound_wait_time_prio_fifo(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long wait_time = 0;
	unsigned long delay_by_lower = 0;
	unsigned long delay_by_same = get_max_CS_per_cluster(info, ti, res_id, locking_prio);

	// choose initial value for wait_time bound: hit by each job with higher locking prio
	// and once by a single lower locking prio request
	foreach(info.get_tasks(), task)
	{
		foreach(task->get_requests(), request)
		{
			if (request->get_resource_id() == res_id &&
				task->get_cluster() != ti.get_cluster()) // ignore local tasks
			{
				if (request->get_request_priority() > locking_prio)
				{
					// this is a lower-prio request; use it to update the max. low-prio CSL
					delay_by_lower = std::max(delay_by_lower, (unsigned long) request->get_request_length());
				}
				else if (request->get_request_priority() < locking_prio)
				{
					// this is a higher-prio request; add total CSL to initial wait time
					wait_time += request->get_request_length() * request->get_num_requests();
				}
			}
		}
	}
	wait_time += delay_by_lower;
	wait_time += delay_by_same;

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
				if (request->get_resource_id() == res_id &&
					task->get_cluster() != ti.get_cluster())
				{
					if (request->get_request_priority() < locking_prio)
					{
						// this is a higher-prio request; add to total CSL
						delay_by_higher += request->get_request_length() * request->get_max_num_requests(estimate);
					}
				}
			}
		}
		new_estimate = delay_by_lower + delay_by_same + delay_by_higher;
	}

	if (estimate <= ti.get_period())
		return estimate; // wait time converged; return wait time
	else
		return -1; // wait time didn't converge; return error value
}

// Constraint 16: Limit spin-delay due to higher-priority requests
// using the wait-time bound.
bool add_prio_fifo_direct_blocking_HP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	std::set<unsigned int> global_resources = get_global_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(global_resources, resource)
	{
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks
		long wait_time_bound = bound_wait_time_prio_fifo(info, ti, *resource, min_prioHP);

		if (wait_time_bound < 0) // use response time if wait-time cannot be bounded
			wait_time_bound = ti.get_response();;

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
						(*request).get_request_priority() < min_prioHP)
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

// Constraint 17: Limit direct blocking due to requests with the same locking priority
// to the number of requests issued by Ti and higher-prio tasks on the same processor
// while Ti is pending for each resource per processor.
// This sounds rather convoluted. Just check out Constraint 17.
void add_prio_fifo_max_direct_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	Clusters clusters;
	split_by_cluster(info, clusters);
	std::set<unsigned int> all_resources = get_all_resources(info);

	foreach(all_resources, resource)
	{
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int niql = count_local_hp_reqs(info, ti, *resource); //count local hp requests for resource
		for (unsigned int c = 0; c < clusters.size(); c++)
		{
			if (c == ti.get_cluster())
				continue;

			LinearExpression *exp = new LinearExpression();
			foreach(clusters[c], task)
			{
				foreach((*task)->get_requests(), request)
				{
					unsigned int res_id = request->get_resource_id();
					if (res_id == *resource &&
						(*request).get_request_priority() == min_prioHP)
					{
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id;
							var_id = vars.lookup((*task)->get_id(), *resource, v, BLOCKING_DIRECT);
							exp->add_var(var_id);
						}
					}
				}
			}
			if (exp->has_terms())
				lp.add_inequality(exp, niql);
			else
				delete exp;
		}
	}
}

// Constraint 18:  Limit arrival-blocking due to higher-priority requests
// using the wait-time bound.
bool add_prio_fifo_arrival_blocking_HP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	std::set<unsigned int> global_resources = get_global_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(global_resources, resource)
	{
		unsigned int min_prioLP = get_min_prio(info, ti, *resource, true);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks
		long wait_time_bound = bound_wait_time_prio_fifo(info, ti, *resource, min_prioLP);

		if (wait_time_bound < 0) // use response time if wait-time cannot be bounded
			wait_time_bound = ti.get_response();;

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
						(*request).get_request_priority() < min_prioLP)
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
					exp->add_term(-1 * max_num_reqs * ncs, var_id);
					lp.add_inequality(exp, 0);
				}
				else
					delete exp;
			}
		}
	}
	return true;
}

// Constraint 19: limit arrival blocking to one request of same locking priority
// per processor per resource, unless disallowed according to definition of A_q.
void add_prio_fifo_atmostonce_remote_arrival_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> global_resources = get_global_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(global_resources, resource)
	{
		unsigned int min_prioLP = get_min_prio(info, ti, *resource, true);

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (c == ti.get_cluster())
				continue;

			LinearExpression *exp = new LinearExpression();
			foreach(clusters[c], task)
			{
				foreach((*task)->get_requests(), request)
				{
					if (request->get_resource_id() == *resource &&
						(*request).get_request_priority() == min_prioLP)
					{
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task)->get_id(), *resource, v, BLOCKING_ARRIVAL);
							exp->add_var(var_id);
						}
					}
				}
			}
			if (exp->has_terms())
			{
				unsigned int var_id = vars.lookup_arrival_enabled(*resource);
				exp->add_term(-1, var_id);
				lp.declare_variable_binary(var_id);
				lp.add_inequality(exp, 0);
			}
			else
				delete exp;
		}
	}
}

bool add_prio_fifo_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	// Constraint 13, Constraint 14
	add_prio_blocking_LP_constraints(vars, info, ti, lp);

	// Constraint 16
	if (!add_prio_fifo_direct_blocking_HP_constraints(vars, info, ti, lp))
		return false;

	// Constraint 17
	add_prio_fifo_max_direct_blocking_constraints(vars, info, ti, lp);

	// Constraint 18
	if (!add_prio_fifo_arrival_blocking_HP_constraints(vars, info, ti, lp))
		return false;

	// Constraint 19
	add_prio_fifo_atmostonce_remote_arrival_constraints(vars, info, ti, lp);

	return true;
}

bool apply_prio_fifo_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info)
{
	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	if (!add_prio_fifo_constraints(vars, info, ti, lp))
		return false;

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

unsigned long lp_prio_fifo_bounds_single(
		const ResourceSharingInfo& info,
		unsigned int task_index)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	unsigned long blocking_term;
	if (!apply_prio_fifo_bounds_for_task(task_index, *results, info))
		blocking_term = ULONG_MAX;
	else
		blocking_term = results->get_blocking_term(task_index);

	delete results;
	return blocking_term;
}

BlockingBounds* lp_prio_fifo_bounds(const ResourceSharingInfo& info)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_prio_fifo_bounds_for_task(i, *results, info);
	}

	return results;
}
