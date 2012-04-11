#ifndef FFDBF_H
#define FFDBF_H

class FFDBFGedf : public SchedulabilityTest
{
  private:
    const unsigned int m;
    const unsigned long epsilon_denom;
    const fractional_t sigma_step;

  private:
    bool witness_condition(const TaskSet &ts,
                           const integral_t q[], const fractional_t r[],
                           const fractional_t &time, const fractional_t &speed);

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
