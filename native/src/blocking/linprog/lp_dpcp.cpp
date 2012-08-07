#include <iostream>
#include <set>
#include <algorithm>

#include "lp_common.h"
#include "blocking.h"
#include "stl-hashmap.h"

#define NO_WAIT_TIME_BOUND (-1)

class MaxWaitTimes
{
private:
	// mapping of resource id to previously computed maximum wait time
	hashmap<unsigned int, long> max_wait_time;

	// parameters for wait time calculation
	const ResourceSharingInfo& info;
	const ResourceLocality& locality;
	const TaskInfo& ti;
	const PriorityCeilings& prio_ceiling;

	void bound_wait_time(unsigned int res_id);

public:
	MaxWaitTimes(const ResourceSharingInfo& _info, const ResourceLocality& _loc,
		     const TaskInfo& _ti, const std::vector<unsigned int>& _pc)
		: info(_info), locality(_loc), ti(_ti), prio_ceiling(_pc)
		{};

	long operator[](unsigned int res_id)
	{
		if (!max_wait_time.count(res_id))
			bound_wait_time(res_id);
		return max_wait_time[res_id];
	}
};

void MaxWaitTimes::bound_wait_time(unsigned int res_id)
{
	unsigned int own_length = 0, delay_by_lower = 0, delay_by_higher = 0;

	unsigned int c = locality[res_id];

	// find ti's maximum request length
	foreach(ti.get_requests(), ti_req)
		if (ti_req->get_resource_id() == res_id)
			own_length = std::max(own_length,
					      ti_req->get_request_length());

	// find maximum lower-priority request length that blocks
	foreach_lowereq_priority_task(info.get_tasks(), ti, tx)
	{
		// on the cluster in which res_id is located
		foreach_request_in_cluster(tx->get_requests(), locality,
					   c, request)
		{
			unsigned int q = request->get_resource_id();
			// can block?
			if (prio_ceiling[q] <= ti.get_priority())
				delay_by_lower = std::max(delay_by_lower,
							  request->get_request_length());
		}
	}

	// response-time analysis to find final maximum wait time

	unsigned long next_estimate = own_length + delay_by_lower;
	unsigned long estimate = 0;

	while (next_estimate <= ti.get_response() && next_estimate != estimate)
	{
		delay_by_higher = 0;
		estimate = next_estimate;

		foreach_higher_priority_task(info.get_tasks(), ti, tx)
		{
			foreach_request_in_cluster(tx->get_requests(), locality,
						   c, request)
			{
				unsigned int nreqs = request->get_max_num_requests(estimate);
				unsigned long rlen = request->get_request_length();

				delay_by_higher += nreqs * rlen;
			}
		}

		next_estimate = own_length + delay_by_lower + delay_by_higher;
	}

	if (estimate <= ti.get_response())
		max_wait_time[res_id] = estimate;
	else
		max_wait_time[res_id] = NO_WAIT_TIME_BOUND;
}

// Constraint 8
// Ti's maximum wait times limit the maximum number of times
// that higher-priority tasks can directly and indirectly delay Ti.
static void add_max_wait_time_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	const PriorityCeilings& prio_ceilings,
	LinearProgram& lp)
{
	MaxWaitTimes max_wait_time = MaxWaitTimes(info, locality, ti, prio_ceilings);

	foreach_higher_priority_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			unsigned int c = locality[q];

			unsigned int max_num_reqs = 0;
			bool bounded = true;

			// Figure out how often and how long ti is waiting in
			// total in cluster c.  To do so, look at each of ti's
			// requests for a resource located in cluster c.

			foreach_request_in_cluster(ti.get_requests(), locality,
						   c, ti_req)
			{
				unsigned int y = ti_req->get_resource_id();
				long wait = max_wait_time[y];

				if (wait == NO_WAIT_TIME_BOUND)
				{
					bounded = false;
					break;
				}

				unsigned int nreqs = request->get_max_num_requests(wait);
				max_num_reqs += nreqs * ti_req->get_num_requests();
			}


			if (bounded)
			{
				LinearExpression *exp = new LinearExpression();
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
					exp->add_var(var_id);
					var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
					exp->add_var(var_id);
				}
				lp.add_inequality(exp, max_num_reqs);
			}
		}
	}
}

// Substitute for Constraint 8
// no direct or indirect blocking due to resources
// on clusters that ti doesn't even access.
static void add_independent_cluster_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{

	//find all clusters that ti accesses
	std::set<int> accessed_clusters;
	foreach(ti.get_requests(), req)
	{
		int c = locality[req->get_resource_id()];
		accessed_clusters.insert(c);
	}

	LinearExpression *exp = new LinearExpression();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			if (accessed_clusters.count(locality[q]) == 0)
			{
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
					exp->add_var(var_id);
					var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
					exp->add_var(var_id);
				}
			}
		}
	}

	lp.add_equality(exp, 0);
}

// Constraint 6
static void add_conflict_set_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	const PriorityCeilings& prio_ceiling,
	LinearProgram& lp)
{
	LinearExpression *exp = new LinearExpression();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			if (prio_ceiling[q] > ti.get_priority())
			{
				// smaller ID <=> higher priority
				// Priority ceiling is lower than ti's priority,
				// so it cannot block ti.
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
					exp->add_var(var_id);
					var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
					exp->add_var(var_id);
				}
			}
		}
	}

	// force blocking fractions to zero
	lp.add_equality(exp, 0);
}

// Constraint 7
// Each request can be directly delayed at most
// once by a lower-priority task.
static void add_atmostonce_lower_prio_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	const PriorityCeilings& priority_ceiling,
	LinearProgram& lp)
{
	// Count the number of times that each cluster is accessed by ti.
	hashmap<unsigned int, unsigned int> per_cluster_counts;
	foreach(ti.get_requests(), req)
		per_cluster_counts[locality[req->get_resource_id()]] += req->get_num_requests();

	// build one constraint for each cluster
	hashmap<unsigned int, LinearExpression *> constraints;

	foreach_lowereq_priority_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			if (priority_ceiling[q] <= ti.get_priority())
			{
				// yes, can block us
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
				}
			}
		}
	}

	// add each per-cluster constraint
	foreach(constraints, it)
		lp.add_inequality(it->second, per_cluster_counts[it->first]);
}

void add_dpcp_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	const PriorityCeilings& prio_ceilings,
	LinearProgram& lp,
	bool use_rta)
{
	// Constraint 1
	add_mutex_constraints(vars, info, ti, lp);
	// Constraint 2
	add_topology_constraints(vars, info, locality, ti, lp);
	// Constraint 3
	add_local_lower_priority_constraints(vars, info, locality, ti, lp);
	// Constraint 7
	add_atmostonce_lower_prio_constraints(vars, info, locality,
					      ti, prio_ceilings, lp);
	// Constraint 6
	add_conflict_set_constraints(vars, info, locality, ti,
				     prio_ceilings, lp);

	if (use_rta)
		// Constraint 8
		add_max_wait_time_constraints(vars, info, locality, ti, prio_ceilings,
					      lp);
	else
		add_independent_cluster_constraints(vars, info, locality, ti, lp);
}

static void apply_dpcp_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const PriorityCeilings& prio_ceilings,
	bool use_rta)
{
	LinearProgram lp;
	VarMapper vars;
	const TaskInfo& ti = info.get_tasks()[i];
	LinearExpression *local_obj = new LinearExpression();

	set_blocking_objective(vars, info, locality, ti, lp, local_obj);

	add_dpcp_constraints(vars, info, locality, ti,
			     prio_ceilings, lp, use_rta);

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


BlockingBounds* lp_dpcp_bounds(const ResourceSharingInfo& info,
			       const ResourceLocality& locality,
			       bool use_rta)
{
	BlockingBounds* results = new BlockingBounds(info);

	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
		apply_dpcp_bounds_for_task(i, *results, info,
					   locality, prio_ceilings, use_rta);

	return results;
}
