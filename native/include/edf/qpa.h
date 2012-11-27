#ifndef QPA_H
#define QPA_H

class QPATest : public SchedulabilityTest
{
 private:
    unsigned int m;

 public:
    QPATest(unsigned int num_processors);

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

};

#endif
