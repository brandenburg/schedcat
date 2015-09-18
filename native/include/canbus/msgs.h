#ifndef CANBUS_MSGS_H
#define CANBUS_MSGS_H

#ifndef SWIG

#include <vector>
#include <algorithm>
#include <math.h>

#include "tasks.h"
#include "time-types.h"

#endif

class CANTask : public Task
{
  private:
    unsigned long priority; /* lower number => higher priority */
    unsigned long taskid; /* remains same across replicas */
    bool critical; /* only inject node faults into critical tasks */

  public:

    /* construction and initialization */
    void init( unsigned long priority = 0,
               unsigned long taskid = 0 )
    {
        this->priority = priority;
        this->taskid = taskid;
        this->critical = false;
    }

    CANTask(unsigned long wcet = 0,
            unsigned long period = 0,
            unsigned long deadline = 0,
            unsigned long priority = 0,
            unsigned long taskid = 0)
            : Task(wcet, period, deadline, 0, 0, 0) 
    {
        init(priority, taskid);
    }

    /* getter / setter */
    unsigned long get_priority() const { return priority; }
    unsigned long get_taskid() const { return taskid; }
    bool is_critical() const { return critical; }
    void set_priority(unsigned long p) { priority = p; }
    void set_taskid(unsigned long tid) { taskid = tid; }
    void set_critical() { critical = true; }
};

typedef std::vector<CANTask> CANTasks;

class CANTaskSet
{
  private:
    CANTasks tasks;
    unsigned int replication_factor;
    unsigned int rprime;
    std::vector<unsigned long> retransmissions;
    std::vector<unsigned long> omissions;
    std::vector<unsigned long> commissions;
    double prob_omissions;
    double prob_commissions;
    double retransmission_rate;
    double host_fault_rate;
    double busrate;
    unsigned long num_ok_rounds;
    unsigned long num_faulty_rounds;

  public:
    CANTaskSet()
    {
        this->prob_omissions = 0;
        this->prob_commissions = 0;
        this->retransmission_rate = 0;
        this->num_ok_rounds = 0;
        this->num_faulty_rounds = 0;
    }

    CANTaskSet(const CANTaskSet &original) : tasks(original.tasks) {}

    virtual ~CANTaskSet() {}

    void add_task(unsigned long wcet, unsigned long period,
                  unsigned long deadline = 0, unsigned long priority = 0,
                  unsigned long taskid = 0)
    {
        tasks.push_back(CANTask(wcet, period, deadline, priority, taskid));
    }
    
    void add_canbus_task(unsigned long wcet, unsigned long period,
                  unsigned long priority, unsigned long taskid)
    {
        tasks.push_back(CANTask(wcet, period, period, priority, taskid));
    }

    void add_retransmission(unsigned long time)
    {
        retransmissions.push_back(time);
    }

    void add_omission(unsigned long job_no)
    {
        omissions.push_back(job_no);
    }

    void add_commission(unsigned long job_no)
    {
        commissions.push_back(job_no);
    }

    void mark_critical_tasks(unsigned long taskid)
    {
        unsigned int replicas = 0;
        for(unsigned int i = 0; i < tasks.size(); i++)
        {
            if (tasks[i].get_taskid() == taskid)
            {
                tasks[i].set_critical();
                replicas ++;
            }
        }

        this->replication_factor = replicas;
    }

    void add_fault_params(double host_fault_rate,
                          double retransmission_rate)
    {
        //this->prob_omissions = prob_omissions;
        //this->prob_commissions = prob_commissions;
        this->retransmission_rate = retransmission_rate;
        this->host_fault_rate = host_fault_rate;
    }

    double get_busrate() { return this->busrate; }
    void set_busrate(double busrate) { this->busrate = busrate; }

    unsigned int get_rprime() { return this->rprime; }
    void set_rprime(unsigned int rprime) { this->rprime = rprime; }
    
    unsigned int get_replication_factor() { return this->replication_factor; }
    unsigned int get_task_count() const { return tasks.size(); }
    std::vector<unsigned long>& get_retransmissions() { return retransmissions; }
    std::vector<unsigned long>& get_omissions() { return omissions; }
    std::vector<unsigned long>& get_commissions() { return commissions; }
    double get_prob_omissions() { return prob_omissions; }
    double get_prob_commissions() { return prob_commissions; }
    double get_retransmission_rate() {return retransmission_rate; }
    double get_host_fault_rate() {return host_fault_rate; }

    unsigned int get_num_distinct_taskids()
    {
        unsigned int max_taskid = 0;
        for (unsigned int i = 0; i < tasks.size(); i++)
        {
            if (tasks[i].get_taskid() > max_taskid)
            {
                max_taskid = tasks[i].get_taskid();
            }
        }
        return max_taskid;
    }
    
    unsigned long get_num_ok_rounds() { return num_ok_rounds; }
    unsigned long get_num_faulty_rounds() { return num_faulty_rounds; }
    void set_num_ok_rounds(unsigned long n) { this->num_ok_rounds = n; }
    void set_num_faulty_rounds(unsigned long n) { this->num_faulty_rounds = n; }

    CANTask& operator[](int idx) { return tasks[idx]; }
    const CANTask& operator[](int idx) const { return tasks[idx]; }

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

    unsigned long get_period_from_taskid(unsigned int idx)
    {
        for (unsigned int i = 0; i <  tasks.size(); i++)
            if (tasks[i].get_taskid() == idx)
                return tasks[i].get_period();
        return -1;
    }
};

#endif
