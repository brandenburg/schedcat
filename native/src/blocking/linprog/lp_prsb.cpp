#include <stdint.h>
#include <cassert>
#include <cmath>
#include <climits>

#include "linprog/model.h"
#include "linprog/varmapperbase.h"
#include "linprog/solver.h"

#include "sharedres_types.h"

#include "iter-helper.h"
#include "stl-helper.h"
#include "stl-io-helper.h"
#include "math-helper.h"

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

#include "lp_global.h"


class GlobalPRSBAnalysis : public GlobalRestrictedSegmentBoostingLP, public GlobalPriorityQueuesLP
{
	// Constraint 26
	void add_prsb_indirect_constraints();

public:
	GlobalPRSBAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalRestrictedSegmentBoostingLP(info, i, number_of_cpus),
		  GlobalPriorityQueuesLP(info, i, number_of_cpus)
	{
		// Protocol-specific constraints
		// Constraint 26
		add_prsb_indirect_constraints();
	}
};

// Constraint 26: the number of times that Tx can cause Ji to incur indirect
// blocking is limited by the number of times that J_i can be directly blocked
// by other tasks
void GlobalPRSBAnalysis::add_prsb_indirect_constraints()

{
	unsigned int total_num_requests = 0; // RHS of C.26

	// RHS
	foreach(all_resources, resource)
	{
		unsigned int res_u = *resource;

		// count the requests of lower-base-priority jobs to res_u
		unsigned int lp_reqs_res_u = 0;
		foreach_lower_priority_task(taskset, ti, tl)
		{
			lp_reqs_res_u += tl->get_num_requests(res_u);
			if (lp_reqs_res_u)
				break; // one is enough
		}

		// (Lemma 13) NDiq = min(1, sum_{lp tasks} Nlq)
		unsigned int num_direc_bloc = std::min(1u, lp_reqs_res_u);

		// get the maximum time that Ji spends on waiting
		// for the requested resource
		unsigned int res_wait_time = resource_wait_time(res_u);

		// check for convergence failure
		if (res_wait_time == UNLIMITED)
		{
			// did not converge, but cannot continue without this bound
			// skip this constraint
			return;
		}

		//calculate the maximum number requests for the resource by
		//higher-priority tasks
		foreach_higher_priority_task(taskset, ti, th)
			foreach_request_for(th->get_requests(), res_u, hreq)
				num_direc_bloc += hreq->get_max_num_requests(res_wait_time);

		total_num_requests += num_direc_bloc * ti.get_num_requests(res_u);
	}

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		LinearExpression *exp = new LinearExpression();

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
				exp->add_var(vars.indirect(x, q, v));
		}

		add_inequality(exp, total_num_requests);
	}
}


BlockingBounds* lp_prsb_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalPRSBAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}

	return results;
}
