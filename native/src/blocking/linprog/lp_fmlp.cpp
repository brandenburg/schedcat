#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>

#include "lp_common.h"
#include "blocking.h"
#include "stl-hashmap.h"

#include "cpu_time.h"

typedef hashmap<unsigned int, unsigned int> BlockingLimits;

// Constraint 14
static void add_fifo_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		// compute direct blocking opportunities, not counting Tx
		BlockingLimits per_resource_remote;

		foreach(ti.get_requests(), req)
			per_resource_remote[req->get_resource_id()] = 0;

		foreach_local_task_except(info.get_tasks(), *tx, ty)
		{
			foreach(ty->get_requests(), req)
			{
				unsigned int u = req->get_resource_id();
				if (per_resource_remote.find(u) !=
				    per_resource_remote.end())
					per_resource_remote[u] += req->get_max_num_requests(ti.get_response());
			}
		}

		unsigned int total_limit = 0;
		foreach(ti.get_requests(), req)
		{
			unsigned int u = req->get_resource_id();
			total_limit += std::min(req->get_num_requests(),
			                        per_resource_remote[u]);
		}

		LinearExpression *exp = new LinearExpression();

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_var(var_id);
			}
		}

		lp.add_inequality(exp, total_limit);
	}
}

// Constraint 13
static void add_total_fifo_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	BlockingLimits &per_cluster_counts)
{

	unsigned int total_num_requests = 0;

	foreach(ti.get_requests(), req)
		total_num_requests += req->get_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		LinearExpression *exp = new LinearExpression();

		//for all requests accessed by tx
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_var(var_id);
				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_var(var_id);
			}
		}

		lp.add_inequality(exp, per_cluster_counts[tx->get_cluster()]);
	}
}

// Constraint 12
static void add_fifo_resource_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	BlockingLimits per_resource_counts;

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

static BlockingLimits count_blocking_opportunities(
	const ResourceSharingInfo &info,
	const TaskInfo& ti)
{
	BlockingLimits per_cluster_counts;

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int c = tx->get_cluster();

		if (per_cluster_counts.find(c) == per_cluster_counts.end())
		{
			// compute direct blocking opportunities on Tx's cluster
			BlockingLimits per_resource_remote;

			foreach(ti.get_requests(), req)
				per_resource_remote[req->get_resource_id()] = 0;

			foreach_local_task(info.get_tasks(), *tx, ty)
			{
				foreach(ty->get_requests(), req)
				{
					unsigned int u = req->get_resource_id();
					if (per_resource_remote.find(u) !=
					    per_resource_remote.end())
						per_resource_remote[u] += req->get_max_num_requests(ti.get_response());
				}
			}

			per_cluster_counts[c] = 0;
			foreach(ti.get_requests(), req)
			{
				unsigned int u = req->get_resource_id();
				per_cluster_counts[c] += std::min(req->get_num_requests(),
							 per_resource_remote[u]);
			}
		}
	}

	return per_cluster_counts;
}

static void add_fmlp_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	// Constraint 1
	add_mutex_constraints(vars, info, ti, lp);
	// Constraint  9
	add_local_higher_priority_constraints_shm(vars, info, ti, lp);
	// Constraint 10
	add_topology_constraints_shm(vars, info, ti, lp);
	// Constraint 11
	add_local_lower_priority_constraints_shm(vars, info, ti, lp);

	BlockingLimits per_cluster_counts;
	per_cluster_counts = count_blocking_opportunities(info, ti);

	// Constraint 12
	add_fifo_resource_constraints(vars, info, ti, lp);
	// Constraint 13
	add_total_fifo_constraints(vars, info, ti, lp, per_cluster_counts);
	// Constraint 14
	add_fifo_cluster_constraints(vars, info, ti, lp);
}

static void apply_fmlp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info)
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

	set_blocking_objective_part_shm(vars, info, ti, lp, local_obj);
	vars.seal();

	add_fmlp_constraints(vars, info, ti, lp);

#if DEBUG_LP_OVERHEADS >= 2
	model_gen_cost.stop();
	std::cout << model_gen_cost << std::endl;
	solver_cost.start();
#endif

	Solution *sol = linprog_solve(lp, vars.get_num_vars());

#if DEBUG_LP_OVERHEADS >= 2
	solver_cost.stop();
	std::cout << solver_cost << std::endl;
#endif
	assert(sol != NULL);

	Interference total, remote, local;

	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	local.total_length = lrint(sol->evaluate(*local_obj));
	remote.total_length = total.total_length - local.total_length;

	bounds[i] = total;
	bounds.set_remote_blocking(i, remote);
	bounds.set_local_blocking(i, local);

	delete local_obj;
	delete sol;
}


static BlockingBounds* _lp_fmlp_bounds(const ResourceSharingInfo& info)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_fmlp_bounds_for_task(i, *results, info);

	return results;
}

BlockingBounds* lp_part_fmlp_bounds(const ResourceSharingInfo& info)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(cpu_costs);

	cpu_costs.start();
#endif

	BlockingBounds *results = _lp_fmlp_bounds(info);

#if DEBUG_LP_OVERHEADS >= 1
	cpu_costs.stop();
	std::cout << cpu_costs << std::endl;
#endif

	return results;
}
