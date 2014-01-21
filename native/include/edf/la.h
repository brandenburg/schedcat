#ifndef LA_H
#define LA_H

class LAGedf : public SchedulabilityTest
{

private:
	unsigned int m;

	bool is_task_schedulable_for_interval(
		const TaskSet &ts,
		unsigned int l,
		unsigned long suspend,
		const integral_t &ilen, /* interval length is xi_l - d_l */
		integral_t &i1,
		integral_t &sum,
		integral_t *idiff,
		integral_t **ptr);

	bool is_task_schedulable_for_suspension_length(
		const TaskSet &ts,
		unsigned int l,
		unsigned long suspend,
		const fractional_t &m_minus_u,
		const fractional_t &test_point_sum,
		const fractional_t &usum);

	integral_t get_max_test_point(
		const TaskSet &ts,
		unsigned int l,
		const fractional_t &m_minus_u,
		const fractional_t &test_point_sum,
		const fractional_t &usum,
		unsigned long suspension);

public:
	LAGedf(unsigned int num_processors) : m(num_processors) {};

	bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);

	static const double MAX_RUNTIME;
};

#endif
