#include "lp_common.h"
#include "math-helper.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>

void set_msrp_blocking_objective(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

// LP-based analysis of spinlock based protocols
// Based on the paper:
// \tbw


// return the minimum locking priority of any request Ti issues for
// resource res_id
// Take care: assumes that Ti indeed accesses that resource, otherwise
// always returns 0
unsigned int get_min_prio(
		const TaskInfo& ti,
		unsigned int res_id)
{
	unsigned int min_prio = 0;
	foreach(ti.get_requests(), request)
	{
		if (request->get_resource_id() == res_id)
			min_prio = std::max(min_prio, request->get_request_priority());
	}
	return min_prio;
}


// determine the minimum locking priority of any local higher/lower-priority
// task accessing the resource identified with res_id. If LP==true, local
// lower-priority tasks are considered, and otherwise higher-priority tasks
// are considered.
unsigned int get_min_prio(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		bool LP)
{
	unsigned int prio = 0;
	bool found_task = false;
	Clusters clusters;
	split_by_cluster(info, clusters);
	foreach(clusters[ti.get_cluster()], task)
	{
		if (( LP && (*task)->get_priority() <= ti.get_priority())  ||
			(!LP && (*task)->get_priority() >  ti.get_priority()))
			continue;

		foreach((*task)->get_requests(), request)
		{
			if ((*request).get_resource_id() == res_id)
			{
				found_task = true;
				prio = std::max(prio, (*request).get_request_priority());
			}
		}
	}
	if (!found_task)
		prio = info.get_tasks().size();
	return prio;
}

// Determine the maximum number of times any job of Ti can be preempted
// by local higher-priority tasks. If the interval parameter is provided,
// this function returns the number of times any job of Ti can be preempted
// by local higher-priority tasks throughout that interval.
unsigned int max_preemptions(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned long interval)
{
	unsigned int preemptions = 0;
	if (interval == 0)
		interval = ti.get_response();
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster() && task->get_priority() < ti.get_priority())
			preemptions += divide_with_ceil(ti.get_response(), task->get_period());
	}
	return preemptions;
}

// Determine the maximum interference Ti can experience from local
// higher-priority tasks during any interval of the length provided.
unsigned long get_hp_interference(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		const unsigned long interval)
{
	unsigned long interference = 0;

	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster() && task->get_priority() < ti.get_priority())
			interference += divide_with_ceil(interval, task->get_period()) * task->get_cost();
	}
	return interference;
}

unsigned int count_requests_while_pending(
		const ResourceSharingInfo& info,
		unsigned long interval,
		unsigned int res_id,
		unsigned int cluster)
{
	unsigned int requests = 0;
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == cluster)
		{
			foreach(task->get_requests(), request)
			{
				if (request->get_resource_id() == res_id)
				{
					requests += request->get_max_num_requests(interval);
				}
			}
		}
	}
	return requests;
}

std::set<unsigned int> get_all_resources(const ResourceSharingInfo& info)
{
	std::set<unsigned int> all_resources;
	foreach(info.get_tasks(), task)
	{
		foreach(task->get_requests(), request)
			all_resources.insert(request->get_resource_id());
	}
	return all_resources;
}


// return all resources accessed by Ti and other local higher-priority tasks.
std::set<unsigned int> get_localHP_resources(const ResourceSharingInfo& info, const TaskInfo& ti)
{
	std::set<unsigned int> Qlh;
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() != ti.get_cluster()
			|| task->get_priority() > ti.get_priority())
			continue;
		foreach(task->get_requests(), request)
			Qlh.insert(request->get_resource_id());
	}
	return Qlh;
}


std::set<unsigned int> get_global_resources(const ResourceSharingInfo& info)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	std::set<unsigned int> global_resources;
	foreach(all_resources, resource)
	{
		std::set<unsigned int> partitions_using_res;
		foreach(info.get_tasks(), task)
		{
			foreach(task->get_requests(), request)
			{
				unsigned int res_id = request->get_resource_id();
				if (res_id == *resource)
					partitions_using_res.insert(task->get_cluster());
			}
			if (partitions_using_res.size()>1)
			{
				global_resources.insert(*resource);
			}
		}
	}
	return global_resources;
}

// corresponds to "ncs(T_i,q)" in paper
unsigned int count_local_hp_reqs(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id)
{
	unsigned int num_requests = 0;
	foreach(ti.get_requests(), request)
	{
		if ((*request).get_resource_id() == res_id)
			num_requests += (*request).get_num_requests();
	}

	foreach(info.get_tasks(), task)
	{
		if (((*task).get_cluster() == ti.get_cluster())
			&& ((*task).get_priority() < ti.get_priority()))
		{
			foreach((*task).get_requests(), request)
			{
				if ((*request).get_resource_id() == res_id)
				{
					num_requests += (*request).get_max_num_requests(ti.get_response());
				}
			}
		}
	}
	return num_requests;
}

void set_spinlock_blocking_objective(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	LinearExpression *obj = lp.get_objective();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			double length = request->get_request_length();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;

				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				obj->add_term(length, var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_ARRIVAL);
				obj->add_term(length, var_id);
			}
		}
	}
}

// Constraint 1: the two kinds of blocking are mutually exclusive
void add_common_mutex_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
			{
				LinearExpression *exp = new LinearExpression();
				unsigned int var_id;

				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_ARRIVAL);
				exp->add_var(var_id);

				lp.add_inequality(exp, 1);
			}
		}
	}
}

// Constraint 2: non-conflicting local resources cannot cause arrival blocking
void add_common_conflict_set_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> non_conflicting_resources;
	ResourceSet local_resources = get_local_resources(info);
	PriorityCeilings ceilings = get_priority_ceilings(info);

	foreach(local_resources, resource)
	{
		if (ceilings[*resource] > ti.get_priority())
			non_conflicting_resources.insert(*resource);
	}

	LinearExpression *exp = new LinearExpression();
	foreach(non_conflicting_resources, resource)
	{
		unsigned int var_id = vars.lookup_arrival_enabled(*resource);
		exp->add_var(var_id);
	}
	if (exp->has_terms())
		lp.add_equality(exp, 0);
	else
		delete exp;
}



// Constraint 3: At most one resource can cause arrival blocking
void add_common_atmostone_arrival_blocking_res_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	if (all_resources.empty())
		return;

	LinearExpression *exp = new LinearExpression();
	std::set<unsigned int>::iterator resource;
	for (resource = all_resources.begin(); resource != all_resources.end(); resource++)
	{
		unsigned int var_id = vars.lookup_arrival_enabled(*resource);
		exp->add_var(var_id);
	}
	lp.add_inequality(exp, 1);
}


// Constraint 4: disallow arrival blocking for resources that are not accessed by Ti or other
// local lower-priority tasks.
void add_common_no_arrival_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);
	LinearExpression *exp = new LinearExpression();

	foreach(all_resources, resource)
	{
		unsigned int local_lower_prio_reqs = 0;
		foreach(info.get_tasks(), task)
		{
			if ((*task).get_cluster() == ti.get_cluster()
				&& (*task).get_priority() > ti.get_priority())
			{
				foreach((*task).get_requests(), request)
				{
					if (request->get_resource_id() == *resource)
						local_lower_prio_reqs += request->get_num_requests();
				}
			}
		}

		unsigned int var_id = vars.lookup_arrival_enabled(*resource);
		lp.declare_variable_binary(var_id);
		if (local_lower_prio_reqs == 0)
			exp->add_var(var_id);
	}
	if (exp->has_terms())
		lp.add_inequality(exp, 0);
	else
		delete exp;
}

// Constraint 5: Disallow arrival blocking due to requests from local
// higher-priority tasks.
void add_common_no_local_higher_priority_arrival_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	// for all local tasks..
	foreach(clusters[ti.get_cluster()], task)
	{
		// skip ti and lower-priority tasks
		if ((*task)->get_priority() < ti.get_priority())
		{
			LinearExpression *exp = new LinearExpression();
			foreach((*task)->get_requests(), request)
			{
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
					exp->add_var(var_id);
				}
			}
			if (exp->has_terms())
				lp.add_inequality(exp, 0);
			else
				delete exp;
		}

	}
}

// Constraint 6: disallow direct blocking by local low-priority tasks.
void add_common_local_direct_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	// for all local tasks..
	foreach(clusters[ti.get_cluster()], task)
	{
		// skip ti and higher-priority tasks
		if ((*task)->get_priority() > ti.get_priority())
		{
			LinearExpression *exp = new LinearExpression();
			foreach((*task)->get_requests(), request)
			{
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_DIRECT);
					exp->add_var(var_id);
				}
			}
			if (exp->has_terms())
				lp.add_inequality(exp, 0);
			else
				delete exp;
		}
	}
}

// Constraint 7: limit arrival blocking to at most one
// over all requests from local low-priority tasks.
void add_common_atmostonce_local_arrival_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		LinearExpression *exp_per_res = new LinearExpression();

		// for all local tasks..
		foreach(info.get_tasks(), task)
		{
			if (task->get_priority() > ti.get_priority() &&
				task->get_cluster() == ti.get_cluster())
			{
				foreach(task->get_requests(), request)
				{
					if (request->get_resource_id() == *resource)
					{
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup(task->get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
							exp_per_res->add_var(var_id);
						}
					}
				}
			}
		}
		if (exp_per_res->has_terms())
		{
			unsigned int var_id = vars.lookup_arrival_enabled(*resource);
			exp_per_res->add_term(-1, var_id);
			lp.add_inequality(exp_per_res, 0);
		}
		else
			delete exp_per_res;
	}
}


// Constraint 20: disallow remote arrival blocking.
void add_common_preemptive_no_remote_arrival_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);

	foreach(all_resources, resource)
	{
		foreach(info.get_tasks(), task)
		{
			LinearExpression *exp = new LinearExpression();
			if ((*task).get_cluster() != ti.get_cluster())
			{
				foreach((*task).get_requests(), request)
				{
					if (request->get_resource_id() == *resource)
					{
						foreach_request_instance(*request, ti, v)
						{
							unsigned int var_id = vars.lookup((*task).get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
							exp->add_var(var_id);
						}
					}
				}
			}
			if (exp->has_terms())
				lp.add_inequality(exp, 0);
			else
				delete exp;
		}
	}
}


void add_common_spinlock_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	// Constraint 1
	add_common_mutex_constraints(vars, info, ti, lp);

	// Constraint 2
	add_common_conflict_set_constraints(vars, info, ti, lp);

	// Constraint 3
	add_common_atmostone_arrival_blocking_res_constraints(vars, info, ti, lp);

	// Constraint 4
	add_common_no_arrival_blocking_constraints(vars, info, ti, lp);

	// Constraint 5
	add_common_no_local_higher_priority_arrival_constraints(vars, info, ti, lp);

	// Constraint 6
	add_common_local_direct_blocking_constraints(vars, info, ti, lp);

	// Constraint 7
	add_common_atmostonce_local_arrival_constraints(vars, info, ti, lp);
}


void add_common_preemptive_spinlock_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	// Constraint XXX - no remote arrival blocking
	add_common_preemptive_no_remote_arrival_blocking_constraints(vars, info, ti, lp);
}

unsigned long apply_baseline_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	bool preemptive)
{
	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	add_common_spinlock_constraints(vars, info, ti, lp);

	set_spinlock_blocking_objective(vars, info, ti, lp);
	vars.seal();

	Solution *sol = linprog_solve(lp, vars.get_num_vars());

	assert(sol != NULL);
	Interference total;
	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;
	delete sol;
	return total.total_length;
}

unsigned long lp_baseline_bounds_single(
		const ResourceSharingInfo& info,
		unsigned int task_index)
{
	BlockingBounds* results = new BlockingBounds(info);

	apply_baseline_bounds_for_task(task_index, *results, info, false);
	unsigned long blocking_term = results->get_blocking_term(task_index);

	delete results;
	return blocking_term;
}

BlockingBounds* lp_baseline_bounds(const ResourceSharingInfo& info)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_baseline_bounds_for_task(i, *results, info, false);
	}

	return results;
}
