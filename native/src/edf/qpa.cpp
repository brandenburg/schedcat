#include <algorithm>
#include <set>
#include <cassert>

#include <iostream>

#include <stdlib.h>
#include <limits.h>

#include "tasks.h"
#include "math-helper.h"
#include "stl-helper.h"
#include "schedulability.h"

#include "edf/qpa.h"

#include <iostream>

QPATest::QPATest(unsigned int num_processors)
{
	if (num_processors != 1)
	{
		// This is a uniprocessor test---complain even in non-debug
		// builds.
		abort();
	}
}

static integral_t edf_busy_interval(const TaskSet &ts)
{
	integral_t interval = 0;
	integral_t total_cost = 0;

	// initial guess: sum of all costs.
	for (unsigned int i = 0; i < ts.get_task_count(); i++)
	interval += ts[i].get_wcet();

	total_cost = interval;
	do {
		interval = total_cost;
		total_cost = 0;
		for (unsigned int i = 0; i < ts.get_task_count(); i++)
		{
			integral_t jobs;
			jobs = divide_with_ceil(interval, ts[i].get_period());
			total_cost += jobs * ts[i].get_wcet();
		}
	} while (interval != total_cost);

	return interval;
}

static integral_t zhang_burns_interval(const TaskSet &ts)
{
	integral_t interval = 0;
	fractional_t total_scaled_delta = 0;
	fractional_t total_util;

	ts.get_utilization(total_util);

	for (unsigned int i = 0; i < ts.get_task_count(); i++)
	{
		integral_t dl  = ts[i].get_deadline();
		integral_t per = ts[i].get_period();
		integral_t delta = dl - per;

		interval = std::max(interval, delta);

		fractional_t util;
		ts[i].get_utilization(util);
		total_scaled_delta += (per - dl) * util;
	}

	total_scaled_delta /= (1 - total_util);

	interval = std::max(interval, round_up(total_scaled_delta));

	return interval;
}

std::set<unsigned long> get_testpoints(const TaskSet &ts,
                                              const integral_t &max_time)
{
	std::set<unsigned long> points;

	// determine all test points
	for (unsigned int i = 0; i < ts.get_task_count(); i++)
	{
		unsigned long time = ts[i].get_deadline();

		for (unsigned long j = 0; time < max_time; j++)
		{
			points.insert(time);
			time += ts[i].get_period();
		}
	}
	return points;
}

static integral_t max_deadline(const Task &task,
                               const integral_t &max_time)
{
	integral_t dl = max_time - task.get_deadline();

	// implicit floor in integer division
	dl /= task.get_period();
	return dl * task.get_period() + task.get_deadline();
}


static unsigned long min_relative_deadline(const TaskSet &ts)
{
	unsigned long dl = ULONG_MAX;

	for (unsigned int i = 0; i < ts.get_task_count(); i++)
		dl = std::min(dl, ts[i].get_deadline());

	return dl;
}

static integral_t get_largest_testpoint(const TaskSet &ts,
					const integral_t &max_time)
{
	integral_t point = 0;

	for (unsigned int i = 0; i < ts.get_task_count(); i++)
	{
		unsigned long dl = ts[i].get_deadline();
		if (dl < max_time)
		{
			integral_t max_dl = max_deadline(ts[i], max_time);
			if (max_dl == max_time)
				max_dl -= ts[i].get_period();
			if (max_dl > point)
				point = max_dl;
		}
	}

	return point;
}

integral_t QPATest::get_demand(integral_t interval, const TaskSet &ts)
{
	integral_t demand;
	ts.bound_demand(interval, demand);
	return demand;
}

integral_t QPATest::get_max_interval(const TaskSet &ts, const fractional_t& util)
{
	integral_t max_interval = edf_busy_interval(ts);

	if (util < 1)
		max_interval = std::min(max_interval, zhang_burns_interval(ts));

	return max_interval;
}

bool QPATest::is_schedulable(const TaskSet &ts, bool check_preconditions)
{
	if (check_preconditions)
	{
		if (!(ts.has_no_self_suspending_tasks()
		      && ts.has_only_feasible_tasks()))
			return false;
	}

	fractional_t util;
	ts.get_utilization(util);

	if (util > 1)
		return false;

	unsigned long min_interval = min_relative_deadline(ts);

	integral_t max_interval = get_max_interval(ts, util);

	integral_t next = get_largest_testpoint(ts, max_interval);
	integral_t demand;
	integral_t interval;

	do
	{
		interval = next;


		demand = get_demand(interval,ts);

		if (demand < interval)
			next = demand;
		else
			next = get_largest_testpoint(ts, interval);

	} while (demand <= interval && demand > min_interval);

	return demand <= min_interval;
}


static void find_feasible_cost_fixpoint(
	const integral_t &interval,
	const integral_t &demand_of_others,
	unsigned long period,
	integral_t &wcet)
{
	integral_t start, slack, base_length;

	slack = interval - demand_of_others;
	base_length = interval + period;

	assert( slack >= 0 );

	integral_t njobs;

	do {
		start = wcet;

		njobs = base_length;
		njobs -= wcet;
		njobs /= period; // implicit floor

		wcet = slack;
		wcet /= njobs; // implicit floor

		assert( wcet <= start );
	} while (start != wcet && wcet > 0);
}


// Assumption: the last-added task in ts caused the task to become infeasible.
// We'll compute the maximum budget that can be accommodated in a C=D scheme.
unsigned long qpa_get_max_C_equal_D_cost(
	const TaskSet &ts,
	unsigned long wcet,
	unsigned long period)
{
	// last task in ts is to be split
	integral_t max_wcet = wcet;

	fractional_t util, util_new_task;
	util_new_task  = wcet;
	util_new_task /= period;

	ts.get_utilization(util);
	util += util_new_task;

	if (util > 1)
	{
		// over-utilized => we need to shrink the WCET.
		util -= 1;
		max_wcet -= round_up(util * period);
		// if we hit zero, then nothing fits on this CPU anymore
		if (max_wcet <= 0)
			return 0;
	}

	// try C=D scheme
	TaskSet ts_with_split(ts);
	ts_with_split.add_task(max_wcet.get_ui(), period, max_wcet.get_ui());

	const unsigned int s = ts_with_split.get_task_count() - 1;

	bool schedulable = false;
	while (!schedulable && max_wcet > 0)
	{
		integral_t max_interval = edf_busy_interval(ts_with_split);
		unsigned long min_interval = min_relative_deadline(ts_with_split);

		ts_with_split.get_utilization(util);
		if (util < 1)
			max_interval = std::min(max_interval, zhang_burns_interval(ts_with_split));

		integral_t next = get_largest_testpoint(ts_with_split, max_interval);
		integral_t demand;
		integral_t interval;

		do
		{
			interval = next;
			ts_with_split.bound_demand(interval, demand);
			if (demand < interval)
				next = demand;
			else
				next = get_largest_testpoint(ts_with_split, interval);

		} while (demand <= interval && demand > min_interval);

		schedulable = demand <= min_interval;

		if (!schedulable)
		{
			// compute largest budget that would have fit and adjust task Ts
			ts.bound_demand(interval, demand); // demand of others without Ts
			find_feasible_cost_fixpoint(interval, demand, period, max_wcet);
			// update task parameters and check again
			ts_with_split[s].set_wcet(max_wcet.get_ui());
			ts_with_split[s].set_deadline(max_wcet.get_ui());
		}
	}

	return max_wcet.get_ui();
}
