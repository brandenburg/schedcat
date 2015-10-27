#include "canbus/job_completion_stats.h"
#include "canbus/can_sim.h"

void CANBusJobCompletionStats::job_completed(int proc, CANJob * job)
{
    if (job->get_task().get_taskid() == taskid &&
        job->get_task().get_priority() == priority &&
        job->get_seqno() == seqno)
    {
        completion_time = current_time;
        abort();
    }
}

unsigned long get_job_completion_time(CANTaskSet &ts, 
                                      simtime_t end_of_simulation,
                                      unsigned long taskid, 
                                      unsigned long priority,
                                      unsigned long seqno)
{
    CANBusJobCompletionStats sim(taskid, priority, seqno);
    run_periodic_simulation(sim, ts, end_of_simulation);
    return sim.get_completion_time();
}
