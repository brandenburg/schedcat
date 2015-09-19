#include "canbus/can_sim.h"

#include <iostream>
#include <stdlib.h>
using namespace std;

void CANJob::reset_params()
{
    release = 0;
    allocation = 0;
    seqno = 1;
    cost = task.get_wcet();
    host_faults.clear();
}

void CANJob::update_seqno(simtime_t time)
{
    unsigned long expected_seqno = ((time + 1) / task.get_period()) + 1;

    if (expected_seqno > get_seqno())
    {
        seqno = expected_seqno;
        release = (expected_seqno - 1) * task.get_period();
    }
}

template<>
void PeriodicCANJobSequence::completed(simtime_t when, int proc)
{
    init_next();
    get_sim()->add_release(this);
}

void run_periodic_simulation(CANBusScheduleSimulation& sim,
                             CANTaskSet& ts,
                             simtime_t end_of_simulation)
{
    PeriodicCANJobSequence** jobs;

    jobs = new PeriodicCANJobSequence*[ts.get_task_count()];
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        jobs[i] = new PeriodicCANJobSequence(ts[i]);
        jobs[i]->set_simulation(&sim);
        sim.add_release(jobs[i]);
    }

    sim.simulate_until(end_of_simulation);

    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        delete jobs[i];
    delete [] jobs;
}

bool CANJob::gen_host_faults(double rate, int max, int boot_time)
{
    if (rate == 0)
        return true;

    //TODO: next should not be of type integer. if rate is too low,
    // then it will overlfow.

    for (int next = (((-1) * log(1 - PROB_RANDOM)) / rate);
         next < max + (boot_time * 2);
         next += (((-1) * log(1 - PROB_RANDOM)) / rate))
    {
        host_faults.push_back(next - (boot_time * 2));
    }

    if (DEBUG_MODE)
    {
        cout << "host faults: ";
        for (unsigned int i = 0; i < host_faults.size(); i++)
            cout << host_faults[i] << " ";
        cout << endl << flush;
    }

    return host_faults.empty();
}

bool CANJob::is_omission(simtime_t boot_time)
{
    int  end = release;
    int  start = release - boot_time; 

    while(!host_faults.empty())
    {
        if (end < host_faults.front())
            return false;
        
        if (start <= host_faults.front() && host_faults.front() <= end)
        {
            // intentionally not erasing here
            return true;
        }

        // if (host_faults.front() < start)
        host_faults.erase(host_faults.begin());
    }

    return false;
}

bool CANJob::is_commission(int start, int end)
{
    while(!host_faults.empty())
    {
        if (end < host_faults.front())
            return false;
        
        if (start <= host_faults.front() && host_faults.front() <= end)
        {
            host_faults.erase(host_faults.begin());
            return true;
        }

        // if (host_faults.front() < start)
        host_faults.erase(host_faults.begin());
    }

    return false;
}

void CANBusScheduler::simulate_until(simtime_t end_of_simulation)
{
    while ( current_time <= end_of_simulation &&
            !is_aborted() &&
            !events.empty() )
    {
        simtime_t next = events.top().time();
        advance_time(next);
    }
}

void CANBusScheduler::advance_time(simtime_t until)
{
    simtime_t last = current_time;
    current_time = until;

    // 1) advance time until next event
    // (job completion or event)
    if (processor->advance_time(current_time - last))
    {
        // process job completion
        CANJob* sched = processor->get_scheduled();
        processor->idle();

        if (is_retransmission(current_time - sched->get_cost(), current_time))
        {
            // notify simulation callback
            job_retransmitted(sched);

            retransmit(sched);
            
            // account for the error frame (with maximum size)
            current_time += EFS;
        }
        else
        {
            // notify simulation callback
            job_completed(0, sched);

            // nofity job callback
            sched->completed(current_time, 0);

            // account for interframe space
            current_time += IFS;
        }
    }

    // 2) process any pending events
    while (!events.empty())
    {
        const Timeout<simtime_t>& next_event = events.top();

        // no more expired events
        if (next_event.time() > current_time)
            break;

        next_event.event().fire(current_time);
        events.pop();
    }

    // 3) schedule if CAN bus is idle
    if (!pending.empty() && !processor->get_scheduled())
    {
        CANJob* job = pending.top();
        pending.pop();
        
        // since CAN bus controllers overwrite jobs
        job->update_seqno(current_time);

        // schedule job
        processor->schedule(job);

        // notify simulation callback
        job_scheduled(0, NULL, job);

        // schedule job completion event
        Timeout<simtime_t> ev(job->remaining_demand() + current_time, &dummy);
        events.push(ev);
    }
}

void CANBusScheduler::retransmit(CANJob *job)
{
    job->set_allocation(0);

    // release immediately
    pending.push(job);

    // notify simulation callback
    job_released(job);
}

void CANBusScheduler::add_ready(CANJob *job)
{
    // the object frees itself in DeadlineEvent::fire()
    DeadlineEvent *handler = new DeadlineEvent(job->get_task(),
                                               job->get_release(),
                                               job->get_seqno(),
                                               job->get_cost(),
                                               this);

    Timeout<simtime_t> ev(job->get_deadline(), handler);
    events.push(ev);

    if (job->get_task().is_critical() && job->is_omission(boot_time))
    {
        // notify simulation callback
        job_omitted(job);

        // notify job callback
        job->completed(current_time, 0);

        return;
    } 

    // release immediately
    pending.push(job);

    // notify simulation callback
    job_released(job);
}

void CANBusScheduler::add_release(SimCANJob *job)
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

void CANBusScheduler::reset_events_and_pending_queues()
{
    while(!events.empty())
        events.pop();

    while(!pending.empty())
        pending.pop();
}

bool CANBusScheduler::gen_retransmissions(double rate, simtime_t max)
{
    if (rate == 0)
        return true;

    for (simtime_t next = (((-1) * log(1 - PROB_RANDOM)) / rate); next < max;
                   next += (((-1) * log(1 - PROB_RANDOM)) / rate))
        retransmissions.push_back(next);

    /*cout << "retr faults: ";
    for (unsigned int i = 0; i < retransmissions.size(); i++)
        cout << retransmissions[i] << " ";
    cout << endl;*/

    return retransmissions.empty();
}

bool CANBusScheduler::is_retransmission(simtime_t start, simtime_t end)
{
    while(!retransmissions.empty())
    {
        if (end < retransmissions.front())
            return false;
        
        if (start <= retransmissions.front() && retransmissions.front() <= end)
        {
            retransmissions.erase(retransmissions.begin());
            return true;
        }

        // if (retransmissions.front() < start)
        retransmissions.erase(retransmissions.begin());
    }

    return false;
}
