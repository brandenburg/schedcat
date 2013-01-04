#include <iostream>
#include <set>
#include <algorithm>
#include <cmath>

#include "lp_common.h"
#include "blocking.h"
#include "stl-hashmap.h"

#include "cpu_time.h"

#include "mpcp.h"

#define NO_BOUND (-1)

typedef hashmap<unsigned int, unsigned int> PerResourceCounts;
typedef hashmap<unsigned int, hashmap<unsigned int, unsigned int> > PerTaskPerRequestDirectBlockingBound;
typedef hashmap<unsigned int, unsigned int> PerTaskIndirectBlockingBound;

class GcsResponseTimes
{
private:
	// per each task, and per each global critical section (gcs),
	// the maximum wait time bound
	hashmap< unsigned long, hashmap< unsigned int, long> > remote_delay;

	hashmap< unsigned long, hashmap< unsigned int, unsigned long> > gcs_response;

	const ResourceSharingInfo &info;
	const MPCPCeilings &prio_ceilings;

	long bound_remote_delay(const TaskInfo &tsk,
			     	unsigned int res_id);

	void bound_gcs_response_times(void);


public:
	GcsResponseTimes(const ResourceSharingInfo &i,
			 const MPCPCeilings &pc)
			 : info(i), prio_ceilings(pc)
	{
		bound_gcs_response_times();
	}


	long get_max_remote_delay(const TaskInfo &ti, unsigned int res_id)
	{
		if (remote_delay.find(ti.get_id()) == remote_delay.end())
			remote_delay[ti.get_id()] = hashmap<unsigned int, long>();

		hashmap<unsigned int, long> &tmap = remote_delay[ti.get_id()];

		if (tmap.find(res_id) == tmap.end())
			tmap[res_id] = bound_remote_delay(ti, res_id);

		return tmap[res_id];
	}

	long get_gcs_response(const TaskInfo &ti, unsigned int res_id)
	{
		// look up the task
		if (gcs_response.find(ti.get_id()) == gcs_response.end())
			return NO_BOUND;

		hashmap<unsigned int, unsigned long> &tmap = gcs_response[ti.get_id()];

		// look up the resource
		if (tmap.find(res_id) == tmap.end())
			return NO_BOUND;

		return tmap[res_id];
	}
};


void GcsResponseTimes::bound_gcs_response_times()
{
	Clusters clusters;
	split_by_cluster(info, clusters);

	ClusterResponseTimes responses;
	determine_gcs_response_times(clusters, prio_ceilings, responses);

	for (unsigned int c = 0; c < clusters.size(); c++)
	{
		const Cluster &cluster = clusters[c];
		const TaskResponseTimes &tasks_response_times = responses[c];

		for (unsigned int i = 0; i < cluster.size(); i++)
		{
			const TaskInfo *ti      = cluster[i];
			const ResponseTimes &ri = tasks_response_times[i];

			gcs_response[ti->get_id()] = hashmap<unsigned int, unsigned long>();
			hashmap<unsigned int, unsigned long> &response = gcs_response[ti->get_id()];

			for (unsigned int r = 0; r < ti->get_requests().size(); r++)
			{
				unsigned int q = ti->get_requests()[r].get_resource_id();
				response[q] = ri[r];
			}
		}
	}
}

// This function computes an upper bound on remote blocking using response-time analysis.
// This corresponds to Equation (3) in LNR:09.
long GcsResponseTimes::bound_remote_delay(const TaskInfo &ti, unsigned int res_id)
{
	unsigned long delay_by_lower = 0, delay_by_equal = 0, delay_by_higher = 0;

	// find maximum lower-priority request span that can block directly
	foreach_lowereq_priority_task_except(info.get_tasks(), ti, tx)
	{
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			if (q == res_id)
			{
				unsigned long resp = get_gcs_response(*tx, q);

				if (tx->get_priority() > ti.get_priority())
					delay_by_lower = std::max(delay_by_lower,
								  resp);
				else
					delay_by_equal += resp;
			}
		}
	}

	// response-time analysis to find final maximum wait time

	unsigned long next_estimate = delay_by_lower + delay_by_equal;
	unsigned long estimate = 0;

	while (next_estimate <= ti.get_response() && next_estimate != estimate)
	{
		delay_by_higher = 0;
		estimate = next_estimate;

		// accumulate direct higher-priority blocking
		foreach_higher_priority_task(info.get_tasks(), ti, tx)
		{
			foreach(tx->get_requests(), request)
			{
				unsigned int q = request->get_resource_id();

				if (res_id == q)
				{
					unsigned int nreqs = request->get_max_num_requests(estimate);
					delay_by_higher += nreqs * get_gcs_response(*tx, q);
				}
			}
		}

		next_estimate = delay_by_lower + delay_by_equal + delay_by_higher;
	}

	if (estimate <= ti.get_response())
		return estimate;
	else
		return NO_BOUND;
}

static unsigned int count_gcs_preemption_opportunities(
	const ResourceSharingInfo& info,
	const RequestBound &req,
	PerTaskPerRequestDirectBlockingBound &db_bounds,
	const MPCPCeilings &prio_ceilings,
	const TaskInfo& ti)
{
	unsigned int count = 0;
	const TaskInfo &tr = *req.get_task();
	unsigned int req_prio = prio_ceilings[req.get_task()->get_cluster()][req.get_resource_id()];

	// Count everything that can be preempted by 'req' and that
	// is also potentially blocking ti.

	foreach_local_task(info.get_tasks(), tr, tx)
	{
		if (tx->get_id() != tr.get_id() &&
		    tx->get_id() != ti.get_id())
		{
			foreach(tx->get_requests(), request)
			{
				unsigned int q = request->get_resource_id();
				if (q != req.get_resource_id() &&
				    db_bounds[tx->get_id()][q] > 0)
				{
					// request can directly block Ti
					unsigned int xprio = prio_ceilings[tx->get_cluster()][q];

					if (xprio >= req_prio)
					{
						// indirect delay is possible
						// each time that Ti is directly blocked by 'request'.
						// This kind of blocking is possible
						count += db_bounds[tx->get_id()][q];
					}
				}
			}
		}
	}

	return count;
}

// Constraint 18
static void add_per_request_indirect_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	PerTaskPerRequestDirectBlockingBound &db_bounds,
	const MPCPCeilings &prio_ceilings,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	PerTaskIndirectBlockingBound bounds;

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			LinearExpression *exp = new LinearExpression();
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_var(var_id);
			}
			unsigned int bound = count_gcs_preemption_opportunities(
				info, *request, db_bounds, prio_ceilings, ti);
			lp.add_inequality(exp, bound);
		}
	}
}

// Constraint 17
static void add_per_task_indirect_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	PerTaskPerRequestDirectBlockingBound &db_bounds,
	const MPCPCeilings &prio_ceilings,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	PerTaskIndirectBlockingBound bounds;

	// initialize
	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		bounds[tx->get_id()] = 0;
	}

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			if (db_bounds[tx->get_id()][q] > 0)
			{
				// note for each local task an opportunity to cause indirect delay

				unsigned int prio = prio_ceilings[tx->get_cluster()][request->get_resource_id()];
				foreach_local_task_except(info.get_tasks(), *tx, tl)
				{
					// see if it has anything to preempt us with
					foreach(tl->get_requests(), lreq)
					{
						if (prio_ceilings[tl->get_cluster()][lreq->get_resource_id()] <= prio)
						{
							// yes, has one, count opportunity per direct blocking
							bounds[tl->get_id()] += db_bounds[tx->get_id()][q];
							break;
						}
					}
				}
			}
		}
	}

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
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
		lp.add_inequality(exp, bounds[t]);
	}
}

static PerResourceCounts get_per_resource_counts(const TaskInfo &ti)
{
	// Count the number of times that each resource is accessed by ti.
	PerResourceCounts per_resource_counts;
	foreach(ti.get_requests(), req)
		per_resource_counts[req->get_resource_id()] += req->get_num_requests();

	return per_resource_counts;
}

// Constraints 15, 16, and 19
static void add_direct_blocking_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	GcsResponseTimes &rta,
	const TaskInfo& ti,
	PerResourceCounts &per_resource_counts,
	LinearProgram& lp,
	PerTaskPerRequestDirectBlockingBound &db_bounds)
{
	// Each request can be directly delayed at most once by a lower-priority task
	// accessing the same resource.
	// Ti is never directly delayed due to resources it does not request,
	// regardless of the priority of the lock holder.

	// build one constraint for each resource.
	hashmap<unsigned int, LinearExpression *> constraints;

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		// Is Tx a higher-priority task?
		bool hiprio = tx->get_priority() < ti.get_priority();

		db_bounds[t] = hashmap<unsigned int, unsigned int>();

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			bool accessed = per_resource_counts.find(q) !=
 	  				per_resource_counts.end();

 	  		db_bounds[t][q] = 0;

			if (!hiprio || !accessed)
			{
				LinearExpression *exp;

				if (constraints.find(q) == constraints.end())
					constraints[q] = new LinearExpression();

				exp = constraints[q];

				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
					exp->add_var(var_id);
				}

				if (accessed)
					// loprio => direct blocking at most once per request
		 	  		db_bounds[t][q] = std::min(
		 	  			request->get_max_num_requests(ti.get_response()),
					        per_resource_counts[q]);
			}
			else
			{
				// higher-priority request, conflicting
				// how many blocking instances per request?
				long interval = rta.get_max_remote_delay(ti, q);
				if (interval != NO_BOUND)
				{
					// can add constraints
					unsigned int request_count;
					// max per request?
					request_count = request->get_max_num_requests(interval);
					// how many requests?
					request_count *= per_resource_counts[q];
					// add constraint for this task

					db_bounds[t][q] = std::min(
						request->get_max_num_requests(ti.get_response()),
					        request_count);

					LinearExpression *exp = new LinearExpression();
					foreach_request_instance(*request, ti, v)
					{
						unsigned int var_id;
						var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
						exp->add_var(var_id);
					}
					lp.add_inequality(exp, request_count);
				}
			}
		}
	}

	// add each per-resource constraint
	foreach(constraints, it)
	{
		unsigned int bound = 0;
		if (per_resource_counts.find(it->first) !=
 	  	    per_resource_counts.end())
 	  		bound = per_resource_counts[it->first];
		lp.add_inequality(it->second, bound);
	}
}

// Constraint 20
static void add_remote_blocking_constraint(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	GcsResponseTimes &rta,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	unsigned long remote_blocking_bound = 0;

	// sum up maximum remote blocking on a per-request basis
	foreach(ti.get_requests(), req)
	{
		unsigned int num = req->get_num_requests();
		unsigned int q   = req->get_resource_id();
		unsigned long rem_blocking = rta.get_max_remote_delay(ti, q);
		remote_blocking_bound += rem_blocking * num;
	}

	LinearExpression *exp = new LinearExpression();

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			double length = request->get_request_length();;

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_term(length, var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_term(length, var_id);
			}
		}
	}

	lp.add_inequality(exp, remote_blocking_bound);
}

static void add_mpcp_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	const MPCPCeilings& prio_ceilings,
	GcsResponseTimes &gcs_response,
	LinearProgram& lp)
{
	PerResourceCounts counts = get_per_resource_counts(ti);
	PerTaskPerRequestDirectBlockingBound db_bounds;

	// Constraint  1
	add_mutex_constraints(vars, info, ti, lp);
	// Constraint  9
	add_local_higher_priority_constraints_shm(vars, info, ti, lp);
	// Constraint 10
	add_topology_constraints_shm(vars, info, ti, lp);
	// Constraint 11
	add_local_lower_priority_constraints_shm(vars, info, ti, lp);
	// Constraints 15, 16, and 19
	add_direct_blocking_constraints(vars, info, gcs_response, ti, counts, lp, db_bounds);
	// Constraint 17
	add_per_task_indirect_constraints(vars, info, db_bounds, prio_ceilings, ti, lp);
	// Constraint 18
	add_per_request_indirect_constraints(vars, info, db_bounds, prio_ceilings, ti, lp);
	// Constraint 20
	add_remote_blocking_constraint(vars, info, gcs_response, ti, lp);
}

static void apply_mpcp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	const MPCPCeilings& prio_ceilings,
	GcsResponseTimes &gcs_response)
{
	LinearProgram lp;
	VarMapper vars;
	const TaskInfo& ti = info.get_tasks()[i];
	LinearExpression *local_obj = new LinearExpression();
	LinearExpression *remote_obj = new LinearExpression();

#if DEBUG_LP_OVERHEADS >= 2
	static DEFINE_CPU_CLOCK(model_gen_cost);
	static DEFINE_CPU_CLOCK(solver_cost);
	static DEFINE_CPU_CLOCK(remote_cost);

	std::cout << "---- " << __FUNCTION__ << " ----" << std::endl;

	model_gen_cost.start();
#endif

	set_blocking_objective_part_shm(vars, info, ti, lp, local_obj, remote_obj);
	vars.seal();

	add_mpcp_constraints(vars, info, ti,
			     prio_ceilings,
			     gcs_response, lp);

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

	bounds[i] = total;
	bounds.set_local_blocking(i, local);

	delete local_obj;
	delete sol;

#if DEBUG_LP_OVERHEADS >= 2
	remote_cost.start();
#endif

	// compute remote blocking maximum
	lp.set_objective(remote_obj);
	sol = linprog_solve(lp, vars.get_num_vars());

	assert(sol != NULL);

	remote.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds.set_remote_blocking(i, remote);

#if DEBUG_LP_OVERHEADS >= 2
	remote_cost.stop();
	std::cout << remote_cost << std::endl;
#endif
	delete sol;
}


static BlockingBounds* _lp_mpcp_bounds(const ResourceSharingInfo& info)
{
	BlockingBounds* results = new BlockingBounds(info);

	MPCPCeilings prio_ceilings = get_mpcp_ceilings(info);
	GcsResponseTimes gcs_response = GcsResponseTimes(info, prio_ceilings);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_mpcp_bounds_for_task(i, *results, info, prio_ceilings, gcs_response);

	return results;
}

BlockingBounds* lp_mpcp_bounds(const ResourceSharingInfo& info)
{
#if DEBUG_LP_OVERHEADS >= 1
	static DEFINE_CPU_CLOCK(cpu_costs);

	cpu_costs.start();
#endif

	BlockingBounds *results = _lp_mpcp_bounds(info);

#if DEBUG_LP_OVERHEADS >= 1
	cpu_costs.stop();
	std::cout << cpu_costs << std::endl;
#endif

	return results;
}
