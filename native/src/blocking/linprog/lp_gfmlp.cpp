#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>

#include "lp_common.h"
#include "blocking.h"
#include "stl-hashmap.h"

#include "cpu_time.h"

typedef hashmap<unsigned int, unsigned int> BlockingLimits;

// reused from partitioned FMLP+ blocking bounds
void add_fifo_resource_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);
void add_total_fifo_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	BlockingLimits &per_cluster_counts);
void add_fifo_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

BlockingLimits count_blocking_opportunities(
	const ResourceSharingInfo &info,
	const TaskInfo& ti);


// Local tasks block at most once in each segment.
void add_per_segment_once_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	// one segment started by job release
	unsigned int num_segments = 1;
	// each request creates two segments
	num_segments += 2 * ti.get_total_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		LinearExpression *exp = new LinearExpression();
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v,
						     BLOCKING_PREEMPT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v,
						     BLOCKING_INDIRECT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v,
						     BLOCKING_DIRECT);
				exp->add_var(var_id);
			}
		}
		lp.add_inequality(exp, num_segments);
	}
}

// Local tasks block at most once in each segment.
void add_total_preemption_limit_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool using_edf)
{
	BlockingLimits per_resource_counts;

	foreach(ti.get_requests(), req)
		per_resource_counts[req->get_resource_id()] = req->get_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		LinearExpression *exp = new LinearExpression();
		unsigned int t = tx->get_id();
		unsigned int requests_per_job = 0;
		unsigned int lower_prio_jobs = tx->get_max_lower_prio_jobs(ti, using_edf);

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			requests_per_job += request->get_num_requests();
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v,
						     BLOCKING_PREEMPT);
				exp->add_var(var_id);
			}
		}
		lp.add_inequality(exp, lower_prio_jobs * requests_per_job);
	}
}

// Local tasks block at most once in each segment.
void add_resource_preemption_limit_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool using_edf)
{
	BlockingLimits per_resource_counts;

	foreach(ti.get_requests(), req)
		per_resource_counts[req->get_resource_id()] = req->get_num_requests();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		unsigned int lower_prio_jobs = tx->get_max_lower_prio_jobs(ti, using_edf);

		foreach(tx->get_requests(), request)
		{
			LinearExpression *exp = new LinearExpression();
			unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v,
						     BLOCKING_PREEMPT);
				exp->add_var(var_id);
			}
			lp.add_inequality(exp, lower_prio_jobs * request->get_num_requests());
		}
	}
}

static void add_gfmlp_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	unsigned int cluster_size,
	bool using_edf)
{
	// Constraint 1 in RTAS'13
	add_mutex_constraints(vars, info, ti, lp);
	// Constraint 10 in RTAS'13
	add_topology_constraints_shm(vars, info, ti, lp);

	BlockingLimits per_cluster_counts;
	per_cluster_counts = count_blocking_opportunities(info, ti);

	// Constraint 12 in RTAS'13
	add_fifo_resource_constraints(vars, info, ti, lp);
	// Constraint 13 in RTAS'13
	add_total_fifo_constraints(vars, info, ti, lp, per_cluster_counts);
	// Constraint 14 in RTAS'14
	add_fifo_cluster_constraints(vars, info, ti, lp);

	add_per_segment_once_constraints(vars, info, ti, lp);
	add_total_preemption_limit_constraints(vars, info, ti, lp, using_edf);
	add_resource_preemption_limit_constraints(vars, info, ti, lp, using_edf);

	if (cluster_size == 1)
	{
		// special case: partitioned scheduling
		// can exploit non-parallelism of lower-priority jobs
		// Constraint 11 in RTAS'13
		add_local_lower_priority_constraints_shm(vars, info, ti, lp);
	}
}

static void apply_gfmlp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	unsigned int cluster_size,
	bool using_edf)
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

	// XXX is this ok for clustered?
	set_blocking_objective_part_shm(vars, info, ti, lp, local_obj);
	vars.seal();

	add_gfmlp_constraints(vars, info, ti, lp, cluster_size, using_edf);

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


static BlockingBounds* _lp_gfmlp_bounds(
	const ResourceSharingInfo& info,
	unsigned int cluster_size,
	bool using_edf)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_gfmlp_bounds_for_task(i, *results, info, cluster_size, using_edf);

	return results;
}

BlockingBounds* lp_gfmlp_bounds(
	const ResourceSharingInfo& info,
	unsigned int cluster_size,
	bool using_edf)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(cpu_costs);

	cpu_costs.start();
#endif

	BlockingBounds *results = _lp_gfmlp_bounds(info, cluster_size, using_edf);

#if DEBUG_LP_OVERHEADS >= 1
	cpu_costs.stop();
	std::cout << cpu_costs << std::endl;
#endif

	return results;
}
