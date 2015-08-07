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

class GlobalFIFONoProgressAnalysis
	: public GlobalNoProgressMechanismLP, public GlobalFIFOQueuesLP
{

public:
	GlobalFIFONoProgressAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalNoProgressMechanismLP(info, i, number_of_cpus),
		  GlobalFIFOQueuesLP(info, i, number_of_cpus)
	{
	}
};

BlockingBounds* lp_no_progress_fifo_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalFIFONoProgressAnalysis lp(info, i, number_of_cpus);
		(*results)[i] = lp.solve();
	}

	return results;
}
