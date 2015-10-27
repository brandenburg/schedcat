#ifndef CANBUS_TARDINESS_STATS
#define CANBUS_TARDINESS_STATS

#include "can_sim.h"

#define DEBUG_OUTPUT(TIME, JOB, JOB_STATUS) \
    if (DEBUG_MODE) \
    { \
        cout << "at time " << TIME << ": "; \
        cout << JOB_STATUS << " job "; \
        cout << JOB->get_task().get_taskid() << "_"; \
        cout << JOB->get_task().get_priority() << "_"; \
        cout << JOB->get_seqno() << endl; \
    } \

// keeps track of the number of messages received
// for each round of the task
struct RoundInfo
{
    unsigned long seqno; // round number j for vector M_j
    unsigned long ok_msgs;
    unsigned long faulty_msgs;
};

// keeps track of the completed rounds, the latest
// round number, and the currently active round
struct TaskIdInfo
{
    unsigned long latest_round_completed;
    unsigned long num_ok_rounds;
    unsigned long num_faulty_rounds;  
    std::vector<RoundInfo> active_rounds;
};

class CANBusTardinessStats: public CANBusScheduler
{
  private:
    std::map<unsigned long, TaskIdInfo> sync_stats;
    std::map<unsigned long, TaskIdInfo> async_stats;
    unsigned int rprime;
    
  public:
    CANBusTardinessStats() : CANBusScheduler() {}

    unsigned int get_rprime() { return rprime; }
    void set_rprime(unsigned int rprime) { this->rprime = rprime; }

    unsigned long get_num_ok_rounds_sync(unsigned long tid)
    {
        return sync_stats.find(tid)->second.num_ok_rounds;
    }

    unsigned long get_num_ok_rounds_async(unsigned long tid)
    {
        return async_stats.find(tid)->second.num_ok_rounds;
    }

    unsigned long get_num_faulty_rounds_sync(unsigned long tid)
    {
        return sync_stats.find(tid)->second.num_faulty_rounds;
    }

    unsigned long get_num_faulty_rounds_async(unsigned long tid)
    {
        return async_stats.find(tid)->second.num_faulty_rounds;
    }

    virtual void job_released(CANJob *job)
    {
        DEBUG_OUTPUT(current_time, job, "released");
    }

    virtual void job_scheduled(int proc, CANJob *preempted, CANJob *scheduled)
    {
        DEBUG_OUTPUT(current_time, scheduled, "scheduled")
    }

    virtual void job_retransmitted(CANJob *job)
    {
        DEBUG_OUTPUT(current_time, job, "RETRANSMITTING")
    }

    virtual void job_omitted(CANJob *job)
    {
        DEBUG_OUTPUT(current_time, job, "OMITTED")
    }

    virtual void job_committed(CANJob *job)
    {
        DEBUG_OUTPUT(current_time, job, "COMMITTED")
    }

    virtual void job_completed(int proc, CANJob *job);
    virtual void job_deadline_expired(CANJob *job);
   
    void init_sync_stats_for_taskid(unsigned long tid); 
    void init_async_stats_for_taskid(unsigned long tid); 
    void reset_sync_stats();
    void reset_async_stats();

    void job_completed_sync(CANJob *job);
    void job_completed_async(CANJob *job);
    void job_deadline_expired_sync(CANJob *job);
    void job_deadline_expired_async(CANJob *job);
};

#endif
