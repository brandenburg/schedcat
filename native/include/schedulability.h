#ifndef SCHEDULABILITY_H
#define SCHEDULABILITY_H

class SchedulabilityTest
{
  public:
    virtual bool is_schedulable(const TaskSet &ts,
                                bool check_preconditions = true) = 0;

    virtual ~SchedulabilityTest() {};
};

#endif
