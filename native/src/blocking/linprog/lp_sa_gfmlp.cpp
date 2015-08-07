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

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

#include "lp_global.h"


class GlobalFMLPAnalysis : public GlobalPrioInheritanceLP, public GlobalFIFOQueuesLP
{
private:
	// Constraint 13
	void add_fmlp_indirect_preemption_constraints();

public:

	GlobalFMLPAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalPrioInheritanceLP(info, i, number_of_cpus),
		  GlobalFIFOQueuesLP(info, i, number_of_cpus)
	{
		// Protocol-specific constraints

		// Constraint 11
		add_pip_fmlp_no_stalling_interference();
		// Constraint 13
		add_fmlp_indirect_preemption_constraints();
	}
};

// Constraint 13: for each resource lq, and each task Tx,
// the sum of indirect and preemption pi-blocking that lower-priority tasks
// causes to a job Ji is bounded by the number of requests that higher-priority
// tasks can issue to this resource under the FMLP.
void GlobalFMLPAnalysis::add_fmlp_indirect_preemption_constraints()
{
	foreach(all_resources, resource)
	{
		unsigned int request_count = 0;

		// the cumulative number of requests for this resource
		// issued by all higher-priority tasks
		foreach_higher_priority_task(taskset, ti, th)
			foreach_request_for(th->get_requests(), *resource, request)
				request_count += request->get_max_num_requests(ti.get_response());

		foreach_lower_priority_task(taskset, ti, tx)
		{
			const unsigned int x = tx->get_id();
			const unsigned int q = *resource;

			foreach_request_for(tx->get_requests(), q, request)
			{
				LinearExpression *exp = new LinearExpression();

				foreach_request_instance(*request, ti, v)
				{
					exp->add_var(vars.indirect(x, q, v));
					exp->add_var(vars.preemption(x, q, v));
				}
				add_inequality(exp, request_count);
			}
		}
	}
}

BlockingBounds* lp_sa_gfmlp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalFMLPAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}

	return results;
}
