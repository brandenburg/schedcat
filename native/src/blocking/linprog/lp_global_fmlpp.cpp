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

class GlobalFMLPPlusAnalysis : public GlobalRestrictedSegmentBoostingLP, public GlobalFIFOQueuesLP
{
private:
	// Constraint 23
	void add_fmlpp_per_segment_constraints();

	// Constraint 24
	void add_fmlpp_direct_indirect_constraints();

	// Constraint 25
	void add_fmlpp_indirect_constraints();

public:
	GlobalFMLPPlusAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalRestrictedSegmentBoostingLP(info, i, number_of_cpus),
		  GlobalFIFOQueuesLP(info, i, number_of_cpus)
	{
		// Protocol-specific constraints

		// Constraint 23
		add_fmlpp_per_segment_constraints();
		// Constraint 24
		add_fmlpp_direct_indirect_constraints();
		// Constraint 25
		add_fmlpp_indirect_constraints();
	}
};

// Constraint 23: each other task causes Ji to incur pi-blocking
// at most once per segment (a total of 2* \sum N_iq + 1 segments) under the FMLP+
void GlobalFMLPPlusAnalysis::add_fmlpp_per_segment_constraints()
{
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		LinearExpression *exp = new LinearExpression();

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				exp->add_var(vars.direct(x, q, v));

				if (x > ti.get_id())
				{
					exp->add_var(vars.indirect(x, q, v));
					exp->add_var(vars.preemption(x, q, v));
				}
			}
		}
		add_inequality(exp, 1 + 2 * ti.get_total_num_requests());
	}
}

// Constraint 24: each other task can cause Ji to incur direct and indirect
// blocking at most \sum \min(N_iu, \sum N_yu^i) times under the FMLP+
void GlobalFMLPPlusAnalysis::add_fmlpp_direct_indirect_constraints()
{
	unsigned int num_times = 0;

	// RHS
	foreach(ti.get_requests(), req)
	{
		unsigned int num_requests = req->get_num_requests();

		unsigned int total_others = 0;
		foreach_task_except(taskset, ti, ty)
		{
			unsigned int njobs = ty->get_max_num_jobs(ti.get_response());
			total_others += njobs * ty->get_num_requests(req->get_resource_id());
		}

		num_times += std::min(num_requests, total_others);
	}

	// LHS
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		LinearExpression *exp = new LinearExpression();

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{
				exp->add_var(vars.direct(x, q, v));

				if (x > ti.get_id())
					exp->add_var(vars.indirect(x, q, v));
			}
		}

		add_inequality(exp, num_times);
	}
}

// Constraint 25: each lower-base-priority task can cause Ji to incur indirect
// blocking at most \sum \min(N_iu, \sum N_yu^i) times under the FMLP+
void GlobalFMLPPlusAnalysis::add_fmlpp_indirect_constraints()
{
	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		unsigned int num_times = 0;

		// RHS
		foreach(ti.get_requests(), req)
		{
			unsigned int num_requests = req->get_num_requests();

			unsigned int total_others = 0;
			foreach_task_except(taskset, ti, ty)
			{
				if (ty->get_id() == x)
					continue;
				unsigned int njobs = ty->get_max_num_jobs(ti.get_response());
				total_others += njobs * ty->get_num_requests(req->get_resource_id());
			}

			num_times += std::min(num_requests, total_others);
		}

		LinearExpression *exp = new LinearExpression();

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
					exp->add_var(vars.indirect(x, q, v));
		}

		add_inequality(exp, num_times);
	}
}

BlockingBounds* lp_global_fmlpp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalFMLPPlusAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}
	return results;
}
