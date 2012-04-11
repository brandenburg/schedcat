#ifndef LOAD_H
#define LOAD_H

class LoadGedf : public SchedulabilityTest
{
 private:
    unsigned int m;
    fractional_t epsilon;

 public:
    LoadGedf(unsigned int num_processors,
             unsigned int milli_epsilon = 100
             ) : m(num_processors), epsilon(milli_epsilon, 1000) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

};

#endif
