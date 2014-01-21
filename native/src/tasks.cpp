#include <algorithm> // for max
#include <string.h>

#include <vector>

#include <iostream>

#include "tasks.h"
#include "task_io.h"

void Task::init(unsigned long wcet,
                unsigned long period,
                unsigned long deadline,
                unsigned long prio_pt,
   			    unsigned long susp,
                unsigned long max_tardiness)
{
    this->wcet     = wcet;
    this->period   = period;
    if (!deadline)
        this->deadline = period; // implicit
    else
        this->deadline = deadline;
    if (!prio_pt)
        this->prio_pt = deadline;
    else
        this->prio_pt = prio_pt;

    this->self_suspension = susp;
    this->tardiness_threshold = max_tardiness;
}

void Task::get_utilization(fractional_t &util) const
{
    // assumes period != 0
    util  = get_wcet();
    util /= get_period();
}

void Task::get_density(fractional_t &density) const
{
    // assumes deadline != 0
    density  = get_wcet();
    density /= get_deadline();
}

std::ostream& operator<<(std::ostream &os, const Task &t)
{
    os << "Task(" << t.get_wcet() << ", " << t.get_period();
    if (!t.has_implicit_deadline())
        os << ", " << t.get_deadline();
    os << ")";
    return os;
}

TaskSet::TaskSet()
{
}

TaskSet::TaskSet(const TaskSet &original) : tasks(original.tasks)
{
}

TaskSet::~TaskSet()
{
}

#define FORALL(i, pred)                             \
    for (unsigned int i = 0; i < tasks.size(); i++) \
    {                                               \
        if (!(pred))                                \
            return false;                           \
    }                                               \
    return true;                                    \

bool TaskSet::has_only_implicit_deadlines() const
{
    FORALL(i, tasks[i].has_implicit_deadline());
}

bool TaskSet::has_only_constrained_deadlines() const
{
    FORALL(i, tasks[i].has_constrained_deadline());
}

bool TaskSet::has_only_feasible_tasks() const
{
    FORALL(i, tasks[i].is_feasible());
}

bool TaskSet::has_no_self_suspending_tasks() const
{
	FORALL(i, !tasks[i].is_self_suspending());
}

void TaskSet::get_utilization(fractional_t &util) const
{
    fractional_t tmp;
    util = 0;
    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_utilization(tmp);
        util += tmp;
    }
}

void TaskSet::get_density(fractional_t &density) const
{
    fractional_t tmp;
    density = 0;
    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_density(tmp);
        density += tmp;
    }
}

void TaskSet::get_max_density(fractional_t &max_density) const
{
    fractional_t tmp;
    max_density = 0;

    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_density(tmp);
        max_density = std::max(max_density, tmp);
    }
}

bool TaskSet::is_not_overutilized(unsigned int num_processors) const
{
    fractional_t util;
    get_utilization(util);
    return util <= num_processors;
}

// Lemma 7 in FBB:06.
unsigned long TaskSet::k_for_epsilon(unsigned int idx,
                                     const fractional_t &epsilon) const
{
    fractional_t bound;
    fractional_t dp_ratio(tasks[idx].get_deadline(),
                       tasks[idx].get_period());

    tasks[idx].get_utilization(bound);
    bound *= tasks.size();
    bound /= epsilon;
    bound -= dp_ratio;

    return (unsigned long) ceil(std::max(0.0, bound.get_d()));
}

void TaskSet::bound_demand(const integral_t &time, integral_t &demand) const
{
	integral_t task_demand;
	demand = 0;
	for (unsigned int i = 0; i < tasks.size(); i++)
	{
		tasks[i].bound_demand(time, task_demand);
		demand += task_demand;
	}
}

void TaskSet::approx_load(fractional_t &load, const fractional_t &epsilon) const
{
    fractional_t density;

    get_density(density);
    get_utilization(load);

    if (density > load)
    {
        // ok, actually have to do the work;
        load += epsilon;

        std::vector<unsigned long> k;
        k.reserve(tasks.size());

        unsigned long total_times = tasks.size();

        for (unsigned int i = 0; i < tasks.size(); i++)
        {
            k[i] = k_for_epsilon(i, epsilon);
            total_times += k[i];
        }

        std::cout << "total_times = " << total_times << std::endl;

        std::vector<integral_t> times;
        times.reserve(total_times);

        // determine all test points
        for (unsigned int i = 0; i < tasks.size(); i++)
        {
            integral_t time = tasks[i].get_deadline();

            for (unsigned long j = 0; j <= k[i]; j++)
            {
                times.push_back(time);
                time += tasks[i].get_period();
            }
        }

        // sort times
        std::sort(times.begin(), times.end());

        // iterate through test points
        integral_t last = 0;

        for (unsigned int t = 0; t < total_times; t++)
        {
            // avoid redundant check
            if (times[t] > last)
            {
                fractional_t load_at_point = 0;
                fractional_t tmp;

                // compute approximate load at point
                for (unsigned int i = 0; i < tasks.size(); i++)
                {
                    tasks[i].approx_load(times[t], tmp, k[i]);
                    load_at_point += tmp;
                }

                // check if we have a new maximum

                if (load_at_point > density)
                {
                    // reached threshold, can stop iteration
                    load = density;
                    return;
                }
                else if (load_at_point > load)
                    load = load_at_point;

                last = times[t];
            }
        }

    }
}
