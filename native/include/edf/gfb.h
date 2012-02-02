#ifndef GFB_H
#define GFB_H

class GFBGedf : public SchedulabilityTest
{
 private:
    unsigned int m;

 public:
    GFBGedf(unsigned int num_processors) : m(num_processors) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

};

#endif
