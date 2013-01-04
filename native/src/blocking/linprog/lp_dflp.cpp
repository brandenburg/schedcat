#include <iostream>
#include <cmath>

#include "lp_common.h"
#include "stl-hashmap.h"

#include "cpu_time.h"

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

#ifdef CONFIG_MERGED_LINPROGS

static BlockingBounds* _lp_dflp_bounds(const ResourceSharingInfo& info,
				       const ResourceLocality& locality)
{
	BlockingBounds *results = new BlockingBounds(info);
	const unsigned int num_tasks = info.get_tasks().size();
	LinearExpression *local_obj = new LinearExpression[num_tasks];
	LinearExpression *remote_obj = new LinearExpression[num_tasks];

	LinearProgram lp;
	unsigned int var_idx = 0;

#if DEBUG_LP_OVERHEADS >= 2
	static DEFINE_CPU_CLOCK(model_gen_cost);
	static DEFINE_CPU_CLOCK(solver_cost);
	static DEFINE_CPU_CLOCK(extract_cost);
	static DEFINE_CPU_CLOCK(total_cost);

	std::cout << "---- " << __FUNCTION__ << " ----" << std::endl;

	model_gen_cost.start();
	total_cost.start();
#endif

	// Generate a "merged" LP.
	for (unsigned int i = 0; i < num_tasks; i++)
	{
		const TaskInfo &ti = info.get_tasks()[i];
		VarMapper vars = VarMapper(var_idx);

		set_blocking_objective(vars, info, locality, ti, lp,
				       local_obj + i, remote_obj + i);
		add_dflp_constraints(vars, info, locality, ti, lp);

		var_idx = vars.get_next_var();
	}

#if DEBUG_LP_OVERHEADS >= 2
	model_gen_cost.stop();
	solver_cost.start();
#endif

	// Solve the big, combined LP.
	Solution *sol = linprog_solve(lp, var_idx);

	assert(sol != NULL);

#if DEBUG_LP_OVERHEADS >= 2
	solver_cost.stop();
	extract_cost.start();
#endif

	// Extract each task's solution.
	for (unsigned int i = 0; i < num_tasks; i++)
	{
		Interference total, remote, local;

		local.total_length = lrint(sol->evaluate(local_obj[i]));
		remote.total_length = lrint(sol->evaluate(remote_obj[i]));
		total.total_length = local.total_length + remote.total_length;

		(*results)[i] = total;
		results->set_remote_blocking(i, remote);
		results->set_local_blocking(i, local);
	}

#if DEBUG_LP_OVERHEADS >= 2
	extract_cost.stop();
	total_cost.stop();
	std::cout << model_gen_cost << std::endl;
	std::cout << solver_cost << std::endl;
	std::cout << extract_cost << std::endl;
	std::cout << total_cost << std::endl;
#endif

	delete sol;
	delete[] local_obj;
	delete[] remote_obj;

	return results;
}


#else // per-task LPs

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

#if DEBUG_LP_OVERHEADS >= 2
	static DEFINE_CPU_CLOCK(model_gen_cost);
	static DEFINE_CPU_CLOCK(solver_cost);

	std::cout << "---- " << __FUNCTION__ << " ----" << std::endl;

	model_gen_cost.start();
#endif

	set_blocking_objective(vars, info, locality, ti, lp, local_obj);

	add_dflp_constraints(vars, info, locality, ti, lp);

#if DEBUG_LP_OVERHEADS >=2
	model_gen_cost.stop();
	std::cout << model_gen_cost << std::endl;
	solver_cost.start();
#endif

	Solution *sol = linprog_solve(lp, vars.get_num_vars());

#if DEBUG_LP_OVERHEADS >=2
	solver_cost.stop();
	std::cout << solver_cost << std::endl;
#endif

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

static BlockingBounds* _lp_dflp_bounds(const ResourceSharingInfo& info,
				       const ResourceLocality& locality)
{
	BlockingBounds *results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_dflp_bounds_for_task(i, *results, info, locality);

	return results;
}

#endif

BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
				const ResourceLocality& locality)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(cpu_costs);

	cpu_costs.start();
#endif

	BlockingBounds *results = _lp_dflp_bounds(info, locality);

#if DEBUG_LP_OVERHEADS >=1
	cpu_costs.stop();
	std::cout << cpu_costs << std::endl;
#endif

	return results;
}
