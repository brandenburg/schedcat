#include "canbus/tardiness_stats.h"

#include <sys/time.h> // gettimeofday

void CANBusTardinessStats::init_sync_stats_for_taskid(unsigned long tid)
{
    if (sync_stats.find(tid) == sync_stats.end())
    {
        TaskIdInfo info;
        info.latest_round_completed = 0;
        info.num_ok_rounds = 0;
        info.num_faulty_rounds = 0;

        sync_stats.insert(std::pair<unsigned long, TaskIdInfo>(tid, info));
    }
}

void CANBusTardinessStats::init_async_stats_for_taskid(unsigned long tid)
{
    if (async_stats.find(tid) == async_stats.end())
    {
        TaskIdInfo info;
        info.latest_round_completed = 0;
        info.num_ok_rounds = 0;
        info.num_faulty_rounds = 0;

        async_stats.insert(std::pair<unsigned long, TaskIdInfo>(tid, info));
    }
}

void CANBusTardinessStats::reset_sync_stats()
{
    std::map<unsigned long, TaskIdInfo>::iterator it;
    for (it = sync_stats.begin(); it != sync_stats.end(); it++)
    {
        TaskIdInfo& info = it->second;
        info.latest_round_completed = 0;
        info.num_ok_rounds = 0;
        info.num_faulty_rounds = 0;
        info.active_rounds.clear();
    }
}

void CANBusTardinessStats::reset_async_stats()
{
    std::map<unsigned long, TaskIdInfo>::iterator it;
    for (it = async_stats.begin(); it != async_stats.end(); it++)
    {
        TaskIdInfo& info = it->second;
        info.latest_round_completed = 0;
        info.num_ok_rounds = 0;
        info.num_faulty_rounds = 0;
        info.active_rounds.clear();
    }
}

void CANBusTardinessStats::job_completed(int proc, CANJob *job)
{
    DEBUG_OUTPUT(current_time, job, "completed");
    job_completed_sync(job);
    job_completed_async(job);
}

void CANBusTardinessStats::job_completed_sync(CANJob *job)
{
    unsigned long tid = job->get_task().get_taskid();
    unsigned long seqno = job->get_seqno();

    std::map<unsigned long, TaskIdInfo>::iterator it = sync_stats.find(tid);
    TaskIdInfo& info = it->second;
    std::vector<RoundInfo>& rounds = info.active_rounds;

    if (seqno <= info.latest_round_completed)
        return;

    if (rounds.empty() || seqno > rounds.back().seqno)
    {
        RoundInfo temp_info = {seqno, 0, 0};
        rounds.push_back(temp_info);
    }

    for (unsigned int i = 0; i < rounds.size(); i++)
    {
        if (rounds[i].seqno == seqno)
        {
            if (job->get_task().is_critical() &&
                job->is_commission(job->get_release(), current_time))
            {
                DEBUG_OUTPUT(current_time, job, "COMMITTED");
                rounds[i].faulty_msgs++;
            }
            else
                rounds[i].ok_msgs++;
            break;
        }
    }
}

void CANBusTardinessStats::job_completed_async(CANJob *job)
{
    unsigned long tid = job->get_task().get_taskid();
    unsigned long seqno = job->get_seqno();

    std::map<unsigned long, TaskIdInfo>::iterator it = async_stats.find(tid);
    TaskIdInfo& info = it->second;
    std::vector<RoundInfo>& rounds = info.active_rounds;

    if (seqno <= info.latest_round_completed)
        return;

    if (rounds.empty() || seqno > rounds.back().seqno)
    {
        RoundInfo temp_info = {job->get_seqno(), 0, 0};
        rounds.push_back(temp_info);
    }

    for (unsigned int i = 0; i < rounds.size(); i++)
    {
        if (rounds[i].seqno == seqno)
        {
            if (job->get_task().is_critical() &&
                job->is_commission(job->get_release(), current_time))
            {
                DEBUG_OUTPUT(current_time, job, "COMMITTED");
                rounds[i].faulty_msgs++;
            }
            else
                rounds[i].ok_msgs++;

            unsigned local_rprime = this->rprime;
            if (!job->get_task().is_critical())
                local_rprime = 1;

            if (rounds[i].ok_msgs >= local_rprime)
            {
                rounds.erase(rounds.begin() + i);
                info.num_ok_rounds++;
                info.latest_round_completed = seqno;
            }

            break;
        }
    }
}

void CANBusTardinessStats::job_deadline_expired(CANJob *job)
{
    DEBUG_OUTPUT(current_time, job, "absolute deadline")

    job_deadline_expired_sync(job);
    job_deadline_expired_async(job);
}

void CANBusTardinessStats::job_deadline_expired_sync(CANJob *job)
{
    unsigned long tid = job->get_task().get_taskid();
    unsigned long seqno = job->get_seqno();

    std::map<unsigned long, TaskIdInfo>::iterator it = sync_stats.find(tid);
    TaskIdInfo& info = it->second;
    std::vector<RoundInfo>& rounds = info.active_rounds;

    if (seqno <= info.latest_round_completed)
        return;
  
    for (unsigned int i = 0; i < rounds.size(); i++)
    {
        if (rounds[i].seqno == seqno)
        {
            if (rounds[i].ok_msgs > rounds[i].faulty_msgs)
                info.num_ok_rounds++;
            else if (rounds[i].ok_msgs == rounds[i].faulty_msgs)
            {
                if (PROB_RANDOM <= 0.5)
                    info.num_faulty_rounds++;
                else
                    info.num_ok_rounds++;
            }
            else
                info.num_faulty_rounds++;

            rounds.erase(rounds.begin() + i);
            info.latest_round_completed = seqno;

            return;
        }
    }
    
    info.num_faulty_rounds++;
    info.latest_round_completed = seqno;
}

void CANBusTardinessStats::job_deadline_expired_async(CANJob *job)
{
    unsigned long tid = job->get_task().get_taskid();
    unsigned long seqno = job->get_seqno();

    std::map<unsigned long, TaskIdInfo>::iterator it = async_stats.find(tid);
    TaskIdInfo& info = it->second;

    if (seqno <= info.latest_round_completed)
        return;
  
    info.num_faulty_rounds++;
    info.latest_round_completed = seqno;
}

void simulate_for_tardiness_stats(CANTaskSet &ts,
                                  simtime_t sim_len_ms,
                                  simtime_t boot_time_ms,
                                  unsigned int iterations)
{
    // seed rand
    timeval t1;
    gettimeofday(&t1, NULL);
    srand(t1.tv_usec * t1.tv_sec);

    // since simulator runs in units of bit-time
    simtime_t sim_len_bit_time = sim_len_ms * ts.get_busrate();
    simtime_t boot_time_bit_time = boot_time_ms * ts.get_busrate();

    // get the failures rates in units of failures/bit-time
    double retransmission_rate = ts.get_retransmission_rate() / ts.get_busrate();
    double host_fault_rate = ts.get_host_fault_rate() / ts.get_busrate();

    CANBusTardinessStats sim;

    // rprime used for aynchronous protocol
    sim.set_rprime(ts.get_rprime());

    // boot_time used by is_omission module
    sim.set_boot_time(boot_time_bit_time);

    // dictionary mapping tasks to their probability of failure,
    // for synchronous and asynchronous protocols, respectively
    std::map<unsigned long, double> prob_failure_sync;
    std::map<unsigned long, double> prob_failure_async;

    // initialize book-keeping structures for all distinct tasks
    unsigned int num_replicas = 0;
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        // assume that one critical task out of all tasks is replicated
        // and that all replicas of this critical task are marked as critical;
        // thus, replication factor = #critical tasks in the task set
        if (ts[i].is_critical())
            num_replicas++;

        // every distinct task has a distinct task id,
        // replicas have the same task id
        unsigned long tid = ts[i].get_taskid();
       
        // initialization happens only if it has not been done before
        // for the corresponding task; thus, multiple calls for task replicas
        // with the same taskid is OK
        sim.init_sync_stats_for_taskid(tid);
        sim.init_async_stats_for_taskid(tid);

        // note that for task replicas, since the task id had already
        // been inserted into the map, the insert function would not
        // insert a new key-value pair, ubt just return an iterator to
        // the existing pair
        prob_failure_sync.insert(std::pair<unsigned long, double> (tid, 0.0));
        prob_failure_async.insert(std::pair<unsigned long, double> (tid, 0.0));
    }
   
    // create a job structure for each task, and add the first job of
    // each task in the pending jobs queue
    PeriodicCANJobSequence** jobs = new PeriodicCANJobSequence*[ts.get_task_count()];
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
    {
        jobs[i] = new PeriodicCANJobSequence(ts[i]);
        jobs[i]->set_simulation(&sim);
        sim.add_release(jobs[i]);
    }

    // the objective is to run the simulation for 'iterations' times; 
    // but for all iterations where the random fault-generator generates
    // a fault-free configuration, the stats are going to remain the same;
    // thus, we keep track of all fault-free iterations, and instead of
    // executing the simulation for each of these iterations, we execute
    // the simulation once, and then use the results in the end for
    // all fault-free simulations
    unsigned long num_fault_free_sims = 0;

    for (unsigned int i = 0; i < iterations; i++)
    {
        // boolean variable to keep track of whether this iteration
        // is fault-free or not
        bool fault_free_sim = true;

        for (unsigned int j = 0; j < ts.get_task_count(); j++)
        {
            // we assume that only critical tasks, i.e., tasks that are
            // replicated, are susceptible to host faults; for these tasks,
            // we randomly generate host fault instants for each task
            // separately (assuming each task is on separate host)
            if (ts[j].is_critical())
            {
                bool zero_host_faults = jobs[j]->gen_host_faults(
                                                        host_fault_rate,
                                                        sim_len_bit_time,
                                                        boot_time_bit_time );

                fault_free_sim = fault_free_sim && zero_host_faults;
            }
        }
        
        // unlike host faults, since all tasks share a single CAN bus, we 
        // generate a single sequence of CAN bus faults, and assume that
        // each of these fault causes a retransmission if some message's
        // transmission overlaps with the fault instant
        bool zero_retransmissions = sim.gen_retransmissions(retransmission_rate,
                                                            sim_len_bit_time);
        fault_free_sim = fault_free_sim && zero_retransmissions;

        if (fault_free_sim)
        {
            num_fault_free_sims++;
        }
        else
        {
            sim.simulate_until(sim_len_bit_time);

            std::map<unsigned long, double>::iterator it;

            for ( it = prob_failure_sync.begin();
                  it != prob_failure_sync.end();
                  it++ )
            {
                const unsigned long & tid = it->first;
                unsigned long faulty = sim.get_num_faulty_rounds_sync(tid);
                unsigned long ok = sim.get_num_ok_rounds_sync(tid);

                double & prob = it->second; 
                prob += ((double)faulty) / (ok + faulty);
            }

            for ( it = prob_failure_async.begin();
                  it != prob_failure_async.end();
                  it++ )
            {
                const unsigned long & tid = it->first;
                unsigned long faulty = sim.get_num_faulty_rounds_async(tid);
                unsigned long ok = sim.get_num_ok_rounds_async(tid);

                double & prob = it->second; 
                prob += ((double)faulty) / (ok + faulty);
            }
        }
        
        // before starting the next iteration, reset all stats-related
        // structures, and all simulator states 
        sim.reset_events_and_pending_queues(); // in GlobalScheduler
        sim.reset_processors(); // in GlobalScheduler
        sim.reset_current_time(); // in GlobalScheduler
        sim.reset_retransmissions(); // in CANBusScheduler
        sim.reset_sync_stats(); // in CANBusTardinessStats
        sim.reset_async_stats(); // in CANBusTardinessStats
       
        // reset all task-specific parameters, and release a job for each task
        // for the next iteration 
        for (unsigned int i = 0; i < ts.get_task_count(); i++)
        {
            jobs[i]->reset_params();
            sim.add_release(jobs[i]);
        }
    }

    // adjust the stats for all the fault-free simulations
    // by running one fault-free simulation
    if (num_fault_free_sims > 0)
    {
        sim.simulate_until(sim_len_bit_time);

        std::map<unsigned long, double>::iterator it;

        for ( it = prob_failure_sync.begin();
              it != prob_failure_sync.end();
              it++ )
        {
            const unsigned long & tid = it->first;
            unsigned long faulty = sim.get_num_faulty_rounds_sync(tid);
            unsigned long ok = sim.get_num_ok_rounds_sync(tid);

            double & prob = it->second; 
            prob += (((double)faulty) / (ok + faulty)) * num_fault_free_sims;
        }

        for ( it = prob_failure_async.begin();
              it != prob_failure_async.end();
              it++ )
        {
            const unsigned long & tid = it->first;
            unsigned long faulty = sim.get_num_faulty_rounds_async(tid);
            unsigned long ok = sim.get_num_ok_rounds_async(tid);

            double & prob = it->second; 
            prob += (((double)faulty) / (ok + faulty)) * num_fault_free_sims;
        }
    }
    
    // normalize the probability based on the number of iterations    
    std::map<unsigned long, double>::iterator it;

    for (it = prob_failure_sync.begin(); it != prob_failure_sync.end(); it++)
    {
        double & prob = it->second; 
        prob /= iterations;
    }

    for (it = prob_failure_async.begin(); it != prob_failure_async.end(); it++)
    {
        double & prob = it->second; 
        prob /= iterations;
    }

    // free memory used for job structres
    for (unsigned int i = 0; i < ts.get_task_count(); i++)
        delete jobs[i];
    delete [] jobs;

    // output final stats
    cout << num_replicas << " ";

    for (it = prob_failure_sync.begin(); it != prob_failure_sync.end(); it++)
    {
        double & prob = it->second; 
        cout << prob << " ";
    }

    for (it = prob_failure_async.begin(); it != prob_failure_async.end(); it++)
    {
        double & prob = it->second; 
        cout << prob << " ";
    }

    cout << endl;
}
