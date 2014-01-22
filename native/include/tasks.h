#ifndef TASKS_H
#define TASKS_H

#ifndef SWIG

#include <vector>
#include <algorithm>

#include <math.h>

#include "time-types.h"

#endif

class Task
{
  private:
    unsigned long period;
    unsigned long wcet;
    unsigned long deadline;
    unsigned long prio_pt;
    unsigned long self_suspension;
    unsigned long tardiness_threshold;

  public:

    /* construction and initialization */
    void init(
        unsigned long wcet,
        unsigned long period,
        unsigned long deadline = 0,
        unsigned long prio_pt = 0,
        unsigned long susp = 0,
        unsigned long max_tardiness = 0
    );
    Task(unsigned long wcet = 0,
         unsigned long period = 0,
         unsigned long deadline = 0,
         unsigned long prio_pt = 0,
         unsigned long susp = 0,
         unsigned long max_tardiness = 0)
    {
        init(wcet, period, deadline, prio_pt, susp, max_tardiness);
    }

    /* getter / setter */
    unsigned long get_period() const { return period;   }
    unsigned long get_wcet() const   { return wcet;     }
    /* defaults to implicit deadline */
    unsigned long get_deadline() const {return deadline; }
    unsigned long get_prio_pt() const { return prio_pt; }
    unsigned long get_self_suspension() const { return self_suspension; };
    unsigned long get_tardiness_threshold() const { return tardiness_threshold; };

    /* properties */

    bool has_implicit_deadline() const
    {
        return deadline == period;
    }

    bool has_constrained_deadline() const
    {
        return deadline <= period;
    }

    bool is_feasible() const
    {
        return get_deadline() >= get_wcet() + get_self_suspension()
            && get_period() >= get_wcet() + get_self_suspension()
            && get_wcet() > 0;
    }

    bool is_self_suspending() const
    {
        return get_self_suspension() > 0;
    }

    void get_utilization(fractional_t &util) const;
    void get_density(fractional_t &density) const;

    // Demand bound function (DBF) and LOAD support.
    // This implements Fisher, Baker, and Baruah's PTAS

    unsigned long bound_demand(unsigned long time) const
    {
        if (time < deadline)
            return 0;
        else
        {
            unsigned long jobs;

            time -= deadline;
            jobs = time / period; // implicit floor in integer division
            jobs += 1;
            return jobs * wcet;
        }
    }

    void bound_demand(const integral_t &time, integral_t &demand) const
    {
        demand = time - deadline;
        if (demand < 0)
            demand = 0;
        else
        {
            demand /= period; // implicit floor in integer division
            demand += 1;
            demand *= wcet;
        }
    }

    // rely on return value optimization
    integral_t dbf(integral_t t) const
    {
        integral_t db;
        bound_demand(t, db);
        return db;
    }

    void bound_load(const integral_t &time, fractional_t &load) const
    {
        integral_t demand;

        if (time > 0)
        {
            bound_demand(time, demand);
            load = demand;
            load /= time;
        }
        else
            load = 0;
    }

    unsigned long approx_demand(unsigned long time, unsigned int k) const
    {
        if (time < k * period + deadline)
            return bound_demand(time);
        else
        {
            double approx = time - deadline;
            approx *= wcet;
            approx /= period;

            return wcet + (unsigned long) ceil(approx);
        }
    }

    void approx_demand(const integral_t &time, integral_t &demand,
                       unsigned int k) const
    {
        if (time < k * period + deadline)
            bound_demand(time, demand);
        else
        {
            integral_t approx;

            approx = time;
            approx -= deadline;
            approx *= wcet;

            mpz_cdiv_q_ui(demand.get_mpz_t(), approx.get_mpz_t(), period);

            demand += wcet;
        }
    }

    void approx_load(const integral_t &time, fractional_t &load,
                     unsigned int k) const
    {
        integral_t demand;

        if (time > 0)
        {
            approx_demand(time, demand, k);
            load = demand;
            load /= time;
        }
        else
            load = 0;
    }
};

typedef std::vector<Task> Tasks;

class TaskSet
{
  private:
    Tasks tasks;

    unsigned long k_for_epsilon(unsigned int idx, const fractional_t &epsilon) const;

  public:
    TaskSet();
    TaskSet(const TaskSet &original);
    virtual ~TaskSet();

    void add_task(unsigned long wcet, unsigned long period,
                  unsigned long deadline = 0, unsigned long prio_pt = 0,
                  unsigned long suspension = 0, unsigned long tardiness_threshold = 0)
    {
        tasks.push_back(Task(wcet, period, deadline,
            prio_pt, suspension, tardiness_threshold));
    }

    unsigned int get_task_count() const { return tasks.size(); }

    Task& operator[](int idx) { return tasks[idx]; }

    const Task& operator[](int idx) const { return tasks[idx]; }

    bool has_only_implicit_deadlines() const;
    bool has_only_constrained_deadlines() const;
    bool has_only_feasible_tasks() const;
    bool has_no_self_suspending_tasks() const;
    bool is_not_overutilized(unsigned int num_processors) const;

    void get_utilization(fractional_t &util) const;
    void get_density(fractional_t &density) const;
    void get_max_density(fractional_t &max_density) const;

    void bound_demand(const integral_t &time, integral_t &demand) const;
    void approx_load(fractional_t &load, const fractional_t &epsilon = 0.1) const;

    /* wrapper for Python access */
    unsigned long get_period(unsigned int idx) const
    {
        return tasks[idx].get_period();
    }

    unsigned long get_wcet(unsigned int idx) const
    {
        return tasks[idx].get_wcet();
    }

    unsigned long get_deadline(unsigned int idx) const
    {
        return tasks[idx].get_deadline();
    }
};



#endif
