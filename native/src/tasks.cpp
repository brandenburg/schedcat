#include <algorithm> // for max
#include <string.h>

#include <vector>

#include <iostream>

#include "tasks.h"
#include "task_io.h"

void Task::init(unsigned long wcet,
		unsigned long period,
		unsigned long deadline)
{
    this->wcet     = wcet;
    this->period   = period;
    if (!deadline)
        this->deadline = period; // implicit
    else
        this->deadline = deadline;
}

bool Task::has_implicit_deadline() const
{
    return deadline == period;
}

bool Task::has_constrained_deadline() const
{
    return deadline <= period;
}

bool Task::is_feasible() const
{
    return get_deadline() >= get_wcet()
        && get_period() >= get_wcet()
        && get_wcet() > 0;
}

void Task::get_utilization(mpq_class &util) const
{
    // assumes period != 0
    util  = get_wcet();
    util /= get_period();
}

void Task::get_density(mpq_class &density) const
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
        if (!pred)                                  \
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

void TaskSet::get_utilization(mpq_class &util) const
{
    mpq_class tmp;
    util = 0;
    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_utilization(tmp);
        util += tmp;
    }
}

void TaskSet::get_density(mpq_class &density) const
{
    mpq_class tmp;
    density = 0;
    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_density(tmp);
        density += tmp;
    }
}

void TaskSet::get_max_density(mpq_class &max_density) const
{
    mpq_class tmp;
    max_density = 0;

    for (unsigned int i = 0; i < tasks.size(); i++)
    {
        tasks[i].get_density(tmp);
        max_density = std::max(max_density, tmp);
    }
}

bool TaskSet::is_not_overutilized(unsigned int num_processors) const
{
    mpq_class util;
    get_utilization(util);
    return util <= num_processors;
}

// Lemma 7 in FBB:06.
unsigned long TaskSet::k_for_epsilon(unsigned int idx,
                                     const mpq_class &epsilon) const
{
    mpq_class bound;
    mpq_class dp_ratio(tasks[idx].get_deadline(),
                       tasks[idx].get_period());

    tasks[idx].get_utilization(bound);
    bound *= tasks.size();
    bound /= epsilon;
    bound -= dp_ratio;

    return (unsigned long) ceil(std::max(0.0, bound.get_d()));
}

void TaskSet::approx_load(mpq_class &load, const mpq_class &epsilon) const
{
    mpq_class density;

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

        std::vector<mpz_class> times;
        times.reserve(total_times);

        // determine all test points
        for (unsigned int i = 0; i < tasks.size(); i++)
        {
            mpz_class time = tasks[i].get_deadline();

            for (unsigned long j = 0; j <= k[i]; j++)
            {
                times.push_back(time);
                time += tasks[i].get_period();
            }
        }

        // sort times
        std::sort(times.begin(), times.end());

        // iterate through test points
        mpz_class last = 0;

        for (unsigned int t = 0; t < total_times; t++)
        {
            // avoid redundant check
            if (times[t] > last)
            {
                mpq_class load_at_point = 0;
                mpq_class tmp;

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
