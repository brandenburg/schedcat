#include "lp_common.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>
#include "cpu_time.h"

// Constraint 21: Limit the number of preemptions that Ti can incur to
// the number of releases of local higher-priority jobs while Ti's job
// was pending.
// Constraint 22: Force number of preemptions that can cause Ti to
// re-request a resource to zero if Ti doesn't access it.
void add_preemptive_fifo_max_preempt_constraints(
		VarMapperSpinlocks & vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	LinearExpression *exp = new LinearExpression();
	foreach(all_resources, resource)
	{
		unsigned int var_id;
		var_id = vars.lookup_max_preemptions(*resource);
		exp->add_var(var_id);
		lp.declare_variable_integer(var_id);

		unsigned int ncs = count_local_hp_reqs(info, ti, *resource);
		if (ncs == 0)
		{
			LinearExpression *not_accessed_res = new LinearExpression();
			not_accessed_res->add_var(var_id);
			lp.add_inequality(not_accessed_res, 0); // Constraint 22
		}
	}
	unsigned int max_preempt = max_preemptions(info, ti);
	if (exp->has_terms()) // Constraint 21
		lp.add_inequality(exp, max_preempt);
	else
		delete exp;
}

// Constraint 8: limit direct blocking for each resource per processor to the number
// requests issued by Ti and higher-prio tasks on the same processor while Ti is pending.
void add_msrp_max_direct_blocking_constraints(
		VarMapperSpinlocks & vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive)
{
	Clusters clusters;
	split_by_cluster(info, clusters);
	std::set<unsigned int> all_resources = get_all_resources(info);

	foreach(all_resources, resource)
	{
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
					if (res_id == *resource)
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
			{
				if (preemptive)
				{
					unsigned int var_id = vars.lookup_max_preemptions(*resource);
					exp->add_term(-1, var_id);
				}
				lp.add_inequality(exp, niql);
			}
			else
				delete exp;

		}
	}
}

// Constraint 9: limit arrival blocking to one per processor per resource,
// unless disallowed according to definition of A_q.
void add_msrp_atmostonce_remote_arrival_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
//	std::set<unsigned int> global_resources = get_global_resources(info);
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	for (unsigned int c=0; c < clusters.size(); c++)
	{
		if (c == ti.get_cluster())
			continue;

		foreach(all_resources, resource)
		{
			LinearExpression *exp = new LinearExpression();
			foreach(clusters[c], task)
			{
				foreach((*task)->get_requests(), request)
				{
					if (request->get_resource_id() == *resource)
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

static void add_preemptive_fifo_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	add_common_preemptive_spinlock_constraints(vars, info, ti, lp);

	add_preemptive_fifo_max_preempt_constraints(vars, info, ti, lp);

	// Constraint 23
	 add_msrp_max_direct_blocking_constraints(vars, info, ti, lp, true);
}


static void add_msrp_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	// Constraint 8
	 add_msrp_max_direct_blocking_constraints(vars, info, ti, lp, false);

	// Constraint 9
	 add_msrp_atmostonce_remote_arrival_constraints(vars, info, ti, lp);
}

unsigned long apply_msrp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	bool preemptive)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(build_model);
	build_model.start();
#endif
	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	if (preemptive)
		add_preemptive_fifo_constraints(vars, info, ti, lp);
	else
		add_msrp_constraints(vars, info, ti, lp);

	set_spinlock_blocking_objective(vars, info, ti, lp);
	vars.seal();
#if DEBUG_LP_OVERHEADS >= 1
	build_model.stop();
	std::cout << build_model << std::endl;
	static DEFINE_CPU_CLOCK(solve_model);
	solve_model.start();
#endif
	Solution *sol = linprog_solve(lp, vars.get_num_vars());
#if DEBUG_LP_OVERHEADS >= 1
	solve_model.stop();
	std::cout << solve_model << std::endl;
	static DEFINE_CPU_CLOCK(parse_result);
	parse_result.start();
#endif

	assert(sol != NULL);
	Interference total;
	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;


#if DEBUG_LP_OVERHEADS >= 1
	parse_result.stop();
	std::cout << parse_result << std::endl;
#endif

	delete sol;
	return total.total_length;

}

unsigned long lp_preemptive_fifo_bounds_single(
		const ResourceSharingInfo& info,
		unsigned int task_index)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	apply_msrp_bounds_for_task(task_index, *results, info, true);
	unsigned long blocking_term = results->get_blocking_term(task_index);

	delete results;
	return blocking_term;
}

BlockingBounds* lp_preemptive_fifo_bounds(const ResourceSharingInfo& info)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_msrp_bounds_for_task(i, *results, info, true);
	}
	return results;
}

unsigned long lp_msrp_bounds_single(
		const ResourceSharingInfo& info,
		unsigned int task_index)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	apply_msrp_bounds_for_task(task_index, *results, info, false);
	unsigned long blocking_term = results->get_blocking_term(task_index);

	delete results;
	return blocking_term;
}

BlockingBounds* lp_msrp_bounds(const ResourceSharingInfo& info)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(solve_full_ts);
	solve_full_ts.start();
#endif

	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_msrp_bounds_for_task(i, *results, info, false);
	}

#if DEBUG_LP_OVERHEADS >= 1
	solve_full_ts.stop();
	std::cout << solve_full_ts << std::endl;
#endif

	return results;
}
