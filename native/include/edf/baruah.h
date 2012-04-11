#ifndef BARUAH_H
#define BARUAH_H

class BaruahGedf : public SchedulabilityTest
{

private:
    unsigned int m;

    bool is_task_schedulable(unsigned int k,
                             const TaskSet &ts,
                             const integral_t &ilen,
                             integral_t &i1,
                             integral_t &sum,
                             integral_t *idiff,
                             integral_t **ptr);

    void get_max_test_points(const TaskSet &ts, fractional_t& m_minus_u,
                             integral_t* maxp);

public:
    BaruahGedf(unsigned int num_processors) : m(num_processors) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

    static const double MAX_RUNTIME;
};

#endif
