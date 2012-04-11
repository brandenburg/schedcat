#ifndef BAKER_H
#define BAKER_H

class BakerGedf : public SchedulabilityTest
{

 private:
    unsigned int m;

 private:
    void beta(const Task &t_i, const Task &t_k, const fractional_t &lambda_k,
              fractional_t &beta_i);
    bool is_task_schedulable(unsigned int k, const TaskSet &ts);

 public:
    BakerGedf(unsigned int num_processors) : m(num_processors) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};

#endif
