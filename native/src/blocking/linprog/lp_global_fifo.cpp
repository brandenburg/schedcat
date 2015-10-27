#include <stdint.h>
#include <cassert>
#include <climits>
#include <cmath>

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

GlobalFIFOQueuesLP::GlobalFIFOQueuesLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: GlobalSuspensionAwareLP(info, task_index, number_of_cpus)
{
	// Constraint 8
	add_fifo_direct_constraints();
}

// Constraint 8: each other task directly delays Ji for at most once
//under FIFO-based protocols.
void GlobalFIFOQueuesLP::add_fifo_direct_constraints()
{
	foreach(all_resources, res_id)
	{
		const unsigned int num_of_requests = ti.get_num_requests(*res_id);

		foreach_task_except(taskset, ti, tx)
		{
			const unsigned int x = tx->get_id();
			const unsigned int q = *res_id;

			foreach_request_for(tx->get_requests(), q, request)
			{
				LinearExpression *exp = new LinearExpression();

				foreach_request_instance(*request, ti, v)
					exp->add_var(vars.direct(x, q, v));

				add_inequality(exp, num_of_requests);
			}
		}
	}
}
