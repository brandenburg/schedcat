
#include "tasks.h"
#include "schedule_sim.h"

Job::Job(const Task &tsk,
         unsigned long relt,
         unsigned long sequence_no,
         unsigned long cst)
    : task(tsk), release(relt), allocation(0), seqno(sequence_no)
{
    if (!cst)
        cost = task.get_wcet();
    else
        cost = cst;
}

void Job::init_next(simtime_t cost,
               simtime_t inter_arrival_time)
{
    allocation = 0;
    /* if cost == 0, then we keep the last cost */
    if (cost != 0)
        this->cost = cost;
    release += task.get_period() + inter_arrival_time;
    seqno++;
}

template<>
void PeriodicJobSequence::completed(simtime_t when, int proc)
{
    init_next();
    get_sim()->add_release(this);
}


void run_periodic_simulation(ScheduleSimulation& sim,
                             TaskSet& ts,
                             simtime_t end_of_simulation)
{
    PeriodicJobSequence** jobs;

    jobs = new PeriodicJobSequence*[ts.get_task_count()];
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        jobs[i] = new PeriodicJobSequence(ts[i]);
        jobs[i]->set_simulation(&sim);
        sim.add_release(jobs[i]);
    }

    sim.simulate_until(end_of_simulation);

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        delete jobs[i];
    delete [] jobs;
}













