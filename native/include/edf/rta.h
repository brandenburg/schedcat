#ifndef RTA_H
#define RTA_H

class RTAGedf : public SchedulabilityTest
{

 private:
    unsigned int m;
    unsigned int max_rounds;
    unsigned int min_delta;

    bool response_estimate(unsigned int k,
                           const TaskSet &ts,
                           unsigned long const *slack,
                           unsigned long response,
                           unsigned long &new_response);

    bool rta_fixpoint(unsigned int k,
                      const TaskSet &ts,
                      unsigned long const *slack,
                      unsigned long &response);

 public:
   RTAGedf(unsigned int num_processors,
           unsigned int min_fixpoint_step = 0,
           unsigned int max_rounds = 25)
         : m(num_processors), max_rounds(max_rounds),
           min_delta(min_fixpoint_step) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};

#endif
