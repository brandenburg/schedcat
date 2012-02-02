#ifndef BCL_ITERATIVE_H
#define BCL_ITERATIVE_H

class BCLIterativeGedf : public SchedulabilityTest
{

 private:
    unsigned int m;
    unsigned int max_rounds;

    bool slack_update(unsigned int k, const TaskSet &ts,
                      unsigned long *slack, bool &ok);

 public:
    BCLIterativeGedf(unsigned int num_processors, unsigned int max_rounds = 0)
        : m(num_processors), max_rounds(max_rounds) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};

#endif
