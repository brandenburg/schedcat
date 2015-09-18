#ifndef SCHED_H
#define SCHED_H

#include "tasks.h"
#include "event.h"
#include <vector>
#include <queue>
#include <algorithm>

typedef unsigned long simtime_t;

class Job {
  protected:
    const Task  &task;
    simtime_t release;
    simtime_t cost;
    simtime_t allocation;
    simtime_t seqno;

  public:
    Job(const Task &tsk,
        simtime_t relt = 0,
        unsigned long sequence_no = 1,
        simtime_t cost = 0);

    const Task& get_task() const { return task; }
    simtime_t get_release()  const { return release; }
    simtime_t get_deadline() const { return release + task.get_deadline(); }
    simtime_t get_cost() const { return cost; }
    simtime_t get_allocation() const { return allocation; }
    unsigned long get_seqno() const { return seqno; }

    void set_release(simtime_t release)
    {
        this->release = release;
    }

    void set_allocation(simtime_t allocation)
    {
        this->allocation = allocation;
    }

    void increase_allocation(simtime_t service_time)
    {
        allocation += service_time;
    }

    bool is_complete() const
    {
        return allocation >= cost;
    }

    simtime_t remaining_demand() const
    {
        return cost - allocation;
    }

    void init_next(simtime_t cost = 0, simtime_t inter_arrival_time = 0);

    // callbacks
    virtual void completed(simtime_t when, int proc) {};
};

template <typename Job, typename SimJob>
class ScheduleSimulationTemplate
{
  public:
    virtual void simulate_until(simtime_t end_of_simulation) = 0;

    virtual void add_release(SimJob *job) = 0;
    virtual void add_ready(Job *job) = 0;
};

class SimJob;

typedef ScheduleSimulationTemplate<Job, SimJob> ScheduleSimulation;

class SimJob : public Job, public Event<simtime_t>
{
  private:
    ScheduleSimulation* sim;

  public:
    SimJob(Task& tsk, ScheduleSimulation* s = NULL) : Job(tsk), sim(s) {};

    void set_simulation(ScheduleSimulation* s) { sim = s; }
    ScheduleSimulation* get_sim() { return sim; }

    void fire(const simtime_t &time)
    {
        sim->add_ready(this);
    }
};

template <typename SimJob, typename Task>
class PeriodicJobSequenceTemplate : public SimJob
{
  public:
    PeriodicJobSequenceTemplate(Task& tsk) : SimJob(tsk) {};
    virtual ~PeriodicJobSequenceTemplate() {};

    // simulator callback
    virtual void completed(simtime_t when, int proc);
};

typedef PeriodicJobSequenceTemplate<SimJob, Task> PeriodicJobSequence;

class EarliestDeadlineFirst {
  public:
    bool operator()(const Job* a, const Job* b)
    {
        if (a && b)
            return a->get_deadline() > b->get_deadline();
        else if (b && !a)
            return true;
        else
            return false;
    }
};

// periodic job sequence

template <typename Job>
class ProcessorTemplate
{
  private:
    Job*      scheduled;

  public:
    ProcessorTemplate() : scheduled(NULL) {}

    Job* get_scheduled() const { return scheduled; };
    void schedule(Job* new_job) { scheduled = new_job; }

    void idle() { scheduled = NULL; }

    bool advance_time(simtime_t delta)
    {
        if (scheduled)
        {
            scheduled->increase_allocation(delta);
            return scheduled->is_complete();
        }
        else
            return false;
    }
};

typedef ProcessorTemplate<Job> Processor;

template <typename JobPriority, typename Processor>
class PreemptionOrderTemplate
{
  public:
    bool operator()(const Processor& a, const Processor& b)
    {
        JobPriority higher_prio;
        return higher_prio(a.get_scheduled(), b.get_scheduled());
    }
};

typedef std::priority_queue<Timeout<simtime_t>,
                            std::vector<Timeout<simtime_t> >,
                            std::greater<Timeout<simtime_t> >
                             > EventQueue;

template <typename JobPriority>
class GlobalScheduler : public ScheduleSimulation
{
    typedef std::priority_queue<Job*,
                                std::vector<Job*>,
                                JobPriority > ReadyQueue;

  private:
    EventQueue events;
    ReadyQueue pending;
    simtime_t  current_time;

    Processor* processors;
    int num_procs;

    JobPriority                   lower_prio;
    PreemptionOrderTemplate<JobPriority, Processor>  first_to_preempt;

    Event<simtime_t> dummy;

    bool aborted;

  private:

    void advance_time(simtime_t until)
    {
        simtime_t last = current_time;

        current_time = until;

        // 1) advance time until next event (job completion or event)
        for (int i = 0; i < num_procs; i++)
            if (processors[i].advance_time(current_time - last))
            {
                // process job completion
                Job* sched = processors[i].get_scheduled();
                processors[i].idle();
                // notify simulation callback
                job_completed(i, sched);
                // nofity job callback
                sched->completed(current_time, i);
            }

        // 2) process any pending events
        while (!events.empty())
        {
            const Timeout<simtime_t>& next_event = events.top();

            if (next_event.time() <= current_time)
            {
                next_event.event().fire(current_time);
                events.pop();
            }
            else
                // no more expired events
                break;
        }

        // 3) process any required preemptions
        bool all_checked = false;
        while (!pending.empty() && !all_checked)
        {
            Job* highest_prio = pending.top();
            Processor* lowest_prio_proc;

            lowest_prio_proc = std::min_element(processors,
                                                processors + num_procs,
                                                first_to_preempt);
            Job* scheduled = lowest_prio_proc->get_scheduled();

            if (lower_prio(scheduled, highest_prio))
            {
                // do a preemption
                pending.pop();


                // schedule
                lowest_prio_proc->schedule(highest_prio);

                // notify simulation callback
                job_scheduled(lowest_prio_proc - processors,
                              scheduled,
                              highest_prio);
                if (scheduled && !scheduled->is_complete())
                    // add back into the pending queue
                    pending.push(scheduled);

                // schedule job completion event
                Timeout<simtime_t> ev(highest_prio->remaining_demand() +
                                      current_time,
                                      &dummy);
                events.push(ev);
            }
            else
                all_checked = true;
        }
    }

  public:
    GlobalScheduler(int num_procs)
    {
        aborted = false;
        current_time = 0;
        this->num_procs = num_procs;
        processors = new Processor[num_procs];
    }

    virtual ~GlobalScheduler()
    {
        delete [] processors;
    }

    simtime_t get_current_time() { return current_time; }

    void abort() { aborted = true; }

    void simulate_until(simtime_t end_of_simulation)
    {
        while (current_time <= end_of_simulation &&
               !aborted &&
               !events.empty()) {
            simtime_t next = events.top().time();
            advance_time(next);
        }
    }

    // Simulation event callback interface
    virtual void job_released(Job *job) {};
    virtual void job_completed(int proc,
                               Job *job) {};
    virtual void job_scheduled(int proc,
                               Job *preempted,
                               Job *scheduled) {};

    // ScheduleSimulation interface
    void add_release(SimJob *job)
    {
        if (job->get_release() >= current_time)
        {
            // schedule future release
            Timeout<simtime_t> rel(job->get_release(), job);
            events.push(rel);
        }
        else
            add_ready(job);
    }

    // ScheduleSimulation interface
    void add_ready(Job *job)
    {
        // release immediately
        pending.push(job);
        // notify callback
        job_released(job);
    }

};



void run_periodic_simulation(ScheduleSimulation& sim,
                             TaskSet& ts,
                             simtime_t end_of_simulation);


#endif
