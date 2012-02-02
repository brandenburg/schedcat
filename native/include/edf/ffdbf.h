#ifndef FFDBF_H
#define FFDBF_H

class FFDBFGedf : public SchedulabilityTest
{
  private:
    const unsigned int m;
    const unsigned long epsilon_denom;
    const mpq_class sigma_step;

  private:
    bool witness_condition(const TaskSet &ts,
                           const mpz_class q[], const mpq_class r[],
                           const mpq_class &time, const mpq_class &speed);

  public:
    FFDBFGedf(unsigned int num_processors,
              unsigned long epsilon_denom = 10,
              unsigned long sigma_granularity = 50)
        :  m(num_processors),
           epsilon_denom(epsilon_denom),
           sigma_step(1, sigma_granularity)
        {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};

#endif
