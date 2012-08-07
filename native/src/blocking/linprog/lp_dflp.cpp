#include <iostream>

#include "lp_common.h"
#include "stl-hashmap.h"

// Constraint 5
// only one blocking request each time a job of T_i
// issues a request, with regard to each cluster and each task.
static void add_fifo_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	hashmap<unsigned int, unsigned int> per_cluster_counts;

	foreach(ti.get_requests(), req)
		per_cluster_counts[locality[req->get_resource_id()]] += req->get_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		// one constraint for each cluster accessed by tx
		hashmap<unsigned int, LinearExpression *> constraints;

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			unsigned int c = locality[q];
			LinearExpression *exp;

			if (constraints.find(c) == constraints.end())
				constraints[c] = new LinearExpression();

			exp = constraints[c];

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_var(var_id);
				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_var(var_id);
			}
		}

		// add each per-cluster constraint
		foreach(constraints, it)
			lp.add_inequality(it->second, per_cluster_counts[it->first]);
	}
}

// Constraint 4
// only one *directly* blocking request each time a job of T_i
// issues a request, with regard to each resource and each task.
static void add_fifo_resource_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	hashmap<unsigned int, unsigned int> per_resource_counts;

	foreach(ti.get_requests(), req)
		per_resource_counts[req->get_resource_id()] = req->get_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		//for all requests accessed by tx
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			LinearExpression *exp = new LinearExpression();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_var(var_id);
			}

			lp.add_inequality(exp, per_resource_counts[q]);
		}
	}
}

void add_dflp_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	// Constraint 1
	add_mutex_constraints(vars, info, ti, lp);
	// Constraint 2
	add_topology_constraints(vars, info, locality, ti, lp);
	// Constraint 3
	add_local_lower_priority_constraints(vars, info, locality, ti, lp);
	// Constraint 4
	add_fifo_resource_constraints(vars, info, locality, ti, lp);
	// Constraint 5
	add_fifo_cluster_constraints(vars, info, locality, ti, lp);
}

static void apply_dflp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality)
{
	LinearProgram lp;
	VarMapper vars;
	const TaskInfo& ti = info.get_tasks()[i];
	LinearExpression *local_obj = new LinearExpression();

	set_blocking_objective(vars, info, locality, ti, lp, local_obj);

	add_dflp_constraints(vars, info, locality, ti, lp);

	Solution *sol = cplex_solve(lp, vars.get_num_vars());

	assert(sol != NULL);

	Interference total, remote, local;

	total.total_length = sol->evaluate(*lp.get_objective());
	local.total_length = sol->evaluate(*local_obj);
	remote.total_length = total.total_length - local.total_length;

	bounds[i] = total;
	bounds.set_remote_blocking(i, remote);
	bounds.set_local_blocking(i, local);

	delete local_obj;
	delete sol;
}

BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
			    const ResourceLocality& locality)
{
	BlockingBounds *results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_dflp_bounds_for_task(i, *results, info, locality);

	return results;
}
