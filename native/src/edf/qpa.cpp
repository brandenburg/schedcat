#include <algorithm>
#include <set>

#include <stdlib.h>
#include <limits.h>

#include "tasks.h"
#include "math-helper.h"
#include "stl-helper.h"
#include "schedulability.h"

#include "edf/qpa.h"

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

	integral_t max_interval = edf_busy_interval(ts);
	unsigned long min_interval = min_relative_deadline(ts);

	if (util < 1)
		max_interval = std::min(max_interval, zhang_burns_interval(ts));

	integral_t next = get_largest_testpoint(ts, max_interval);
	integral_t demand;
	integral_t interval;

	do
	{
		interval = next;
		ts.bound_demand(interval, demand);
		if (demand < interval)
			next = demand;
		else
			next = get_largest_testpoint(ts, interval);

	} while (demand <= interval && demand > min_interval);

	return demand <= min_interval;
}
