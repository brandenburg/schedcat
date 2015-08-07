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


class GlobalPrioNoProgressAnalysis
	: public GlobalNoProgressMechanismLP, public GlobalPriorityQueuesLP
{

public:
	GlobalPrioNoProgressAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalNoProgressMechanismLP(info, i, number_of_cpus),
		  GlobalPriorityQueuesLP(info, i, number_of_cpus)
	{
	}
};

BlockingBounds* lp_no_progress_priority_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalPrioNoProgressAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}

	return results;
}
