#include "tasks.h"
#include "edf/sim.h"

#include "schedule_sim.h"

#include <algorithm>

typedef GlobalScheduler<EarliestDeadlineFirst> GedfSim;

class DeadlineMissSearch : public GedfSim
{
  private:
    bool dmissed;

  public:
    simtime_t when_missed;
    simtime_t when_completed;

    DeadlineMissSearch(int m) : GedfSim(m), dmissed(false) {};

    virtual void job_completed(int proc, Job *job)
    {
        if (this->get_current_time() > job->get_deadline())
        {
            dmissed = true;
            when_missed    = job->get_deadline();
            when_completed = this->get_current_time();
            abort();
        }
    };

    bool deadline_was_missed()
    {
        return dmissed;
    }
};

class Tardiness : public GedfSim
{
  public:
    Stats stats;

    Tardiness(int m) : GedfSim(m)
    {
        stats.num_tardy_jobs = 0;
        stats.num_ok_jobs = 0;
        stats.total_tardiness = 0;
        stats.max_tardiness = 0;
        stats.first_miss = 0;
    };

    virtual void job_completed(int proc, Job *job)
    {
        if (this->get_current_time() > job->get_deadline())
        {
            simtime_t tardiness;
            tardiness = this->get_current_time() - job->get_deadline();
            stats.num_tardy_jobs++;
            stats.total_tardiness += tardiness;
            stats.max_tardiness = std::max(tardiness, stats.max_tardiness);
            if (!stats.first_miss)
                stats.first_miss = job->get_deadline();
        }
        else
            stats.num_ok_jobs++;
    };
};

unsigned long edf_first_violation(unsigned int num_procs,
                                  TaskSet &ts,
                                  unsigned long end_of_simulation)
{
    DeadlineMissSearch sim(num_procs);

    run_periodic_simulation(sim, ts, end_of_simulation);
    if (sim.deadline_was_missed())
        return sim.when_missed;
    else
        return 0;
}

bool edf_misses_deadline(unsigned int num_procs,
                         TaskSet &ts,
                         unsigned long end_of_simulation)
{
    DeadlineMissSearch sim(num_procs);

    run_periodic_simulation(sim, ts, end_of_simulation);
    return sim.deadline_was_missed();
}


Stats edf_observe_tardiness(unsigned int num_procs,
                            TaskSet &ts,
                            unsigned long end_of_simulation)
{
    Tardiness sim(num_procs);

    run_periodic_simulation(sim, ts, end_of_simulation);

    return sim.stats;
}

