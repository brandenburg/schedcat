#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>

#include "lp_common.h"
#include "blocking.h"
#include "stl-hashmap.h"

#include "cpu_time.h"

// Per-cluster, per-resource access counts.
typedef hashmap<unsigned int, unsigned int> AccessCounts;
typedef hashmap<unsigned int, AccessCounts> PerClusterACounts;

static AccessCounts count_accesses(
	const ResourceSharingInfo& info,
	unsigned int cluster)
{
	AccessCounts acount;

	foreach(info.get_tasks(), tx) {
		if (tx->get_cluster() == cluster) {
			foreach(tx->get_requests(), request)
			{
				unsigned int q = request->get_resource_id();
				// count that q is being accessed by tx
				acount[q] += 1;
			}
		}
	}
	return acount;
}

static void add_total_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	unsigned int num_procs)
{
	// one constraint for each resource
	hashmap<unsigned int, LinearExpression *> constraints;
	LinearExpression *exp;

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		//for all requests accessed by tx
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			if (constraints.find(q) == constraints.end())
				constraints[q] = new LinearExpression();
			exp = constraints[q];
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_SOB);
				exp->add_var(var_id);
			}
		}
	}

	// add each per-resource constraint
	foreach(constraints, it) {
		unsigned int bound = ti.get_num_requests(it->first);
		bound *= (2 * num_procs - 1);
		lp.add_inequality(it->second, bound);
	}
}


static void add_remote_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	AccessCounts& acount,
	unsigned int num_procs,
	unsigned int cluster_size)
{
	// one constraint for each cluster and each resource
	hashmap<unsigned int, hashmap<unsigned int, LinearExpression *> > cluster_c;
	LinearExpression *exp;

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		unsigned int c = tx->get_cluster();

		hashmap<unsigned int, LinearExpression *> &constraints = cluster_c[c];

		//for all requests accessed by tx
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			if (constraints.find(q) == constraints.end())
				constraints[q] = new LinearExpression();
			exp = constraints[q];

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_SOB);
				exp->add_var(var_id);
			}
		}
	}

	// add each per-resource constraint
	foreach(cluster_c, ct) {
		assert(ct->first != ti.get_cluster());
		foreach(ct->second, it) {
			unsigned int bound  = ti.get_num_requests(it->first);
			unsigned int q = it->first;
			if (acount[q] <= 2 * cluster_size) {
				// FQ-only case
				bound *= acount[q];
			} else {
				// PQ case
				bound *= (cluster_size + num_procs);
			}
			lp.add_inequality(it->second, bound);
		}
	}
}

static void add_local_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	AccessCounts& acount,
	unsigned int cluster_size)
{
	foreach_local_task_except(info.get_tasks(), ti, tx)
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
				var_id = vars.lookup(t, q, v, BLOCKING_SOB);
				exp->add_var(var_id);
			}
			unsigned int bound = ti.get_num_requests(q);
			if (acount[q] > 2 * cluster_size)
				bound *= 2; // PQ case
			lp.add_inequality(exp, bound);
		}
	}
}


static void add_omip_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	AccessCounts& acounts,
	unsigned int num_procs,
	unsigned int cluster_size)
{
	add_total_constraints(vars, info, ti, lp,  num_procs);
	add_remote_cluster_constraints(vars, info, ti, lp, acounts, num_procs, cluster_size);
	add_local_cluster_constraints(vars, info, ti, lp, acounts, num_procs);
}

static void apply_omip_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	PerClusterACounts &pcacounts,
	unsigned int num_procs,
	unsigned int cluster_size)
{
	LinearProgram lp;
	VarMapper vars;
	const TaskInfo& ti = info.get_tasks()[i];
	unsigned int cluster = ti.get_cluster();

	if (pcacounts.find(cluster) == pcacounts.end())
		pcacounts[cluster] = count_accesses(info, cluster);

#if DEBUG_LP_OVERHEADS >= 2
	static DEFINE_CPU_CLOCK(model_gen_cost);
	static DEFINE_CPU_CLOCK(solver_cost);

	std::cout << "---- " << __FUNCTION__ << " ----" << std::endl;

	model_gen_cost.start();
#endif

	set_blocking_objective_sob(vars, info, ti, lp);
	vars.seal();

	add_omip_constraints(vars, info, ti, lp,
		pcacounts[cluster], num_procs, cluster_size);

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

	Interference total;

	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;

	delete sol;
}


static BlockingBounds* _lp_omip_bounds(
	const ResourceSharingInfo& info,
	unsigned int num_procs,
	unsigned int cluster_size)
{
	PerClusterACounts pcacounts;
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_omip_bounds_for_task(i, *results, info, pcacounts, num_procs, cluster_size);

	return results;
}

BlockingBounds* lp_omip_bounds(
	const ResourceSharingInfo& info,
	unsigned int num_procs,
	unsigned int cluster_size)
{
	assert(num_procs >= cluster_size);
	assert(num_procs % cluster_size == 0);

#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(cpu_costs);

	cpu_costs.start();
#endif

	BlockingBounds *results = _lp_omip_bounds(info, num_procs, cluster_size);

#if DEBUG_LP_OVERHEADS >= 1
	cpu_costs.stop();
	std::cout << cpu_costs << std::endl;
#endif

	return results;
}
