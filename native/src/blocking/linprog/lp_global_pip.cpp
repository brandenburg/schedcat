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


class GlobalPIPAnalysis : public GlobalPrioInheritanceLP, public GlobalPriorityQueuesLP
{

public:
	GlobalPIPAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalPrioInheritanceLP(info, i, number_of_cpus),
		  GlobalPriorityQueuesLP(info, i, number_of_cpus)
	{
		// Protocol-specific constraints

		// Constraint 11
		add_pip_fmlp_no_stalling_interference();
		// Constraint 12
		add_pip_ppcp_indirect_preemption_constraints();
	}
};


BlockingBounds* lp_global_pip_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalPIPAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}

	return results;
}
