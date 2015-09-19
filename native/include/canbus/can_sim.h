#ifndef CANBUS_SIM_H
#define CANBUS_SIM_H

#include "canbus/msgs.h"
#include "schedule_sim.h"
#include "event.h"

#include <vector>
#include <queue>
#include <list>
#include <algorithm>
#include <map>
#include <iostream>

#define DEBUG_MODE 0

#define IFS 3 // interframe space = 3 bit-time
#define EFS 29 // max error frame size = 29 bit-time

#define BIG_RAND_MAX ((((double)RAND_MAX + 1) * ((double)RAND_MAX + 1)) - 1)
#define BIG_RAND ((rand() * ((double)RAND_MAX + 1)) + rand())
#define PROB_RANDOM (BIG_RAND / BIG_RAND_MAX)

using namespace std;

class CANJob : public Job {
  protected:
    const CANTask  &task;
    std::vector<simtime_t> omissions;
    std::vector<simtime_t> commissions;
    std::vector<int> host_faults;

  public:
    CANJob(const CANTask &tsk,
           simtime_t relt = 0,
           unsigned long sequence_no = 1,
           simtime_t cst = 0)
           : Job(tsk, relt, sequence_no, cst), task(tsk) {}

    const CANTask& get_task() const { return task; }
    simtime_t get_priority() const { return task.get_priority(); }
    simtime_t get_taskid() const { return task.get_taskid(); }

    // callbacks added for CAN bus simulation
    virtual void omitted() {};
   
    // for CAN bus simulation, to have the effect of CAN controllers,
    // i.e., a new job instance overwrites an old job instance of the same task
    void update_seqno(simtime_t time);
 
    void reset_params();
    void init_retransmission(simtime_t when);
    bool gen_host_faults(double rate, int max, int boot_time);
    bool is_omission(simtime_t boot_time);
    bool is_commission(int start, int end);
};

class FixedPriorityScheduling
{
  public:
    // does b have a higher priority than a?
    bool operator()(const CANJob* a, const CANJob* b)
    {
        if (a && b)
        {
            // note: lower number => higher priority
            return a->get_priority() > b->get_priority();
        }

        // if one of them is not defined
        // then return true iff b is defined
        return b;
    }
};

class SimCANJob;
typedef ScheduleSimulationTemplate<CANJob, SimCANJob> CANBusScheduleSimulation;

class SimCANJob : public CANJob, public Event<simtime_t>
{
  protected:
    CANBusScheduleSimulation* sim;

  public:
    SimCANJob(CANTask& tsk, CANBusScheduleSimulation* s = NULL) : CANJob(tsk), sim(s) {};
    
    SimCANJob(const CANTask& tsk,
           unsigned long release,
           unsigned long seqno,
           unsigned long cost,
           CANBusScheduleSimulation* s) :
           CANJob(tsk, release, seqno, cost),
           sim(s) {}

    virtual ~SimCANJob() {}

    void set_simulation(CANBusScheduleSimulation* s) { sim = s; }
    CANBusScheduleSimulation* get_sim() { return sim; }

    void fire(const simtime_t &time)
    {
        sim->add_ready(this);
    }
};

typedef PeriodicJobSequenceTemplate<SimCANJob, CANTask> PeriodicCANJobSequence;
typedef ProcessorTemplate<CANJob> CANBus;
typedef PreemptionOrderTemplate<FixedPriorityScheduling, CANBus> CANBusPreemptionOrder;

void run_periodic_simulation(CANBusScheduleSimulation& sim,
                             CANTaskSet& ts,
                             simtime_t end_of_simulation);

/* CANBusScheduler is essentially a uniprocessor, non-preemptive
scheduler. In addition to the basic scheduler simulation, it simulates
fault-injection as well, including the effect of transmission faults, 
omission faults, and commission faults. */
class CANBusScheduler : public CANBusScheduleSimulation
{
    typedef std::priority_queue<CANJob*,
                                std::vector<CANJob*>,
                                FixedPriorityScheduling> ReadyQueue;
  protected:

    EventQueue events;
    ReadyQueue pending;
    simtime_t current_time;
    simtime_t boot_time;

    CANBus* processor;

    FixedPriorityScheduling lower_prio;
    CANBusPreemptionOrder first_to_preempt;

    Event<simtime_t> dummy;

    bool aborted;

    std::vector<simtime_t> retransmissions;

    bool is_retransmission(simtime_t, simtime_t);
    void advance_time(simtime_t);

  public:
    CANBusScheduler()
    {
        aborted = false;
        current_time = 0;
        processor = new CANBus();
    }

    virtual ~CANBusScheduler()
    {
        delete processor;
    }

    unsigned long get_events_size() { return events.size(); }
    unsigned long get_pending_size() { return pending.size(); }
    simtime_t get_current_time() { return current_time; }
    void set_current_time(simtime_t t) { current_time = t; }
    void set_boot_time(simtime_t t) { boot_time = t; }
    bool is_aborted() { return aborted; }
    void abort() { aborted = true; }
    void reset_current_time() { current_time = 0; }
    void reset_processors() { processor->idle(); }
    void reset_retransmissions() { retransmissions.clear(); }

    void simulate_until(simtime_t end_of_simulation);
    void add_ready(CANJob *job);
    void add_release(SimCANJob *job);
    void reset_events_and_pending_queues();
    bool gen_retransmissions(double rate, simtime_t max);

    virtual void retransmit(CANJob *job);
    // simulation event callback interface
    virtual void job_released(CANJob *job) {};
    virtual void job_completed(int proc, CANJob *job) {};
    virtual void job_scheduled(int proc, CANJob *preempted, CANJob *scheduled) {};
    virtual void job_omitted(CANJob * job) {};
    virtual void job_committed(CANJob *job) {};
    virtual void job_retransmitted(CANJob * job) {};
    virtual void job_deadline_expired(CANJob* job) {};
};

class DeadlineEvent : public SimCANJob
{
  public:
    DeadlineEvent(const CANTask &task,
                  unsigned long release,
                  unsigned long seqno,
                  unsigned long cost,
                  CANBusScheduleSimulation *s) :
                  SimCANJob(task, release, seqno, cost, s) {};
    
    void fire(const simtime_t &time)
    {
        ((CANBusScheduler *)sim)->job_deadline_expired(this);
        delete this;
    }
};

#endif
