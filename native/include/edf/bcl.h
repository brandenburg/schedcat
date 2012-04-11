#ifndef BCL_H
#define BCL_H

class BCLGedf : public SchedulabilityTest
{

 private:
    unsigned int m;

 private:
    unsigned long max_jobs_contained(const Task &t_i, const Task &t_k);
    void beta(const Task &t_i, const Task &t_k, fractional_t &beta_i);
    bool is_task_schedulable(unsigned int k, const TaskSet &ts);

 public:
    BCLGedf(unsigned int num_processors) : m(num_processors) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};

#endif
