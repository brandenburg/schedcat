#ifndef QPA_H
#define QPA_H

class QPATest : public SchedulabilityTest
{
 public:
    QPATest(unsigned int num_processors);

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

    virtual integral_t get_demand(integral_t interval, const TaskSet &ts);
    virtual integral_t get_max_interval(const TaskSet &ts, const fractional_t& util);
};

// support for C=D semi-partitioning assignment heuristic
unsigned long qpa_get_max_C_equal_D_cost(
	const TaskSet &ts,
	unsigned long wcet,
	unsigned long period);


#endif
