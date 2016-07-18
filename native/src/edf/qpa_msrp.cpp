#include "edf/qpa_msrp.h"
#include "sharedres_types.h"
#include "blocking.h"

#include "iter-helper.h"

#include <algorithm>

#include <iostream>
using namespace std;

static unsigned long max_relative_deadline(const TaskSet &ts)
{
	unsigned long dl = 0;

	for (unsigned int i = 0; i < ts.get_task_count(); i++)
		dl = std::max(dl, ts[i].get_deadline());

	return dl;
}

QPA_MSRPTest::QPA_MSRPTest(unsigned int num_processors, const ResourceSharingInfo& rsinfo,
                           unsigned int _num_cpus, unsigned int _cpu_id) // Needed by msrp_bounds
: QPATest(num_processors), num_cpus(_num_cpus), cpu_id(_cpu_id), info(rsinfo)
{}


integral_t QPA_MSRPTest::get_demand(integral_t interval, const TaskSet &ts)
{
	integral_t demand = QPATest::get_demand(interval,ts);

	if (interval <= max_relative_deadline)
		demand += get_EDF_arrival_blocking(info, num_cpus, interval.get_ui(), cpu_id);

	return demand;
}

integral_t QPA_MSRPTest::get_max_interval(const TaskSet &ts, const fractional_t& util)
{
	integral_t max_interval = QPATest::get_max_interval(ts, util);

	// Follows Baruah RTSS'06 - "Resource sharing in EDF-scheduled systems: a closer look"
	max_interval = std::max(max_interval.get_ui(), max_relative_deadline);

	return max_interval;
}


// ------------------------------------------------------------------
// --------------------[ E N T R Y    P O I N T ]--------------------
// ------------------------------------------------------------------

bool pedf_msrp_classic_is_schedulable(const ResourceSharingInfo& info, unsigned int num_cpus)
{
	bool esit = true;

	BlockingBounds* blocking = msrp_bounds(info, num_cpus);

	foreach_cluster(info, k)
	{

		TaskSet ts;

		// Prepare a TaskSet object for each processor:
		// This is to adapt ResourceSharingInfo to the data structures used
		// by the QPA implementation.
		foreach_task_in_cluster(info.get_tasks(), k, T_i)
		{
			ts.add_task(T_i->get_cost() + blocking->get_remote_blocking(T_i->get_id()), // WCET inflation
			T_i->get_period(), T_i->get_deadline());
		}

		QPA_MSRPTest test(1, info, num_cpus, k);
		test.set_max_relative_deadline(max_relative_deadline(ts));

		if (!test.is_schedulable(ts, false))
		{
			esit = false;
			break;
		}
	}

	// The object is allocated into msrp_bound()
	delete blocking;

	return esit;
}
