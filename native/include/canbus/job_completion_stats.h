#ifndef CANBUS_JOB_COMPLETION_STATS
#define CANBUS_JOB_COMPLETION_STATS

#include "canbus/can_sim.h"

/* Wrapper around the CANBusScheduler simulator class to collect specific
stats just for testing purposes. */
class CANBusJobCompletionStats: public CANBusScheduler
{
  private:
    unsigned long taskid;
    unsigned long priority;
    unsigned long seqno; // job id    
    simtime_t completion_time;

  public:
    CANBusJobCompletionStats(unsigned long taskid,
                                unsigned long priority,
                                unsigned long seqno) : 
                                CANBusScheduler (),
                                taskid (taskid),
                                priority (priority),
                                seqno (seqno) {}

    unsigned long  get_completion_time() { return completion_time; }
    virtual void job_completed(int proc, CANJob *job);
};

#endif
