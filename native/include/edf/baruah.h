#ifndef BARUAH_H
#define BARUAH_H

class BaruahGedf : public SchedulabilityTest
{

private:
    unsigned int m;

    bool is_task_schedulable(unsigned int k,
                             const TaskSet &ts,
                             const mpz_class &ilen,
                             mpz_class &i1,
                             mpz_class &sum,
                             mpz_class *idiff,
                             mpz_class **ptr);

    void get_max_test_points(const TaskSet &ts, mpq_class& m_minus_u,
                             mpz_class* maxp);

public:
    BaruahGedf(unsigned int num_processors) : m(num_processors) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

    static const double MAX_RUNTIME;
};

#endif
