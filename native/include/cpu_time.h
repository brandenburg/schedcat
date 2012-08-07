#ifndef CPU_TIME_H
#define CPU_TIME_H

#include <iostream>

// How much CPU time used (in seconds)?
double get_cpu_usage(void);

class CPUClock
{
private:
	const char *name;
	const char *func;

	unsigned int count;

	double start_time;
	double last;
	double total;

public:
	CPUClock(const char *_name = 0, const char *_func = 0)
		: name(_name), func(_func),
		  count(0), start_time(0), last(0), total(0)
	{}

	void start()
	{
		start_time = get_cpu_usage();
	}

	void stop()
	{
		last = get_cpu_usage() - start_time;
		total += last;
		count++;
	}

	double get_total() const
	{
		return total;
	}

	double get_last() const
	{
		return last;
	}

	double get_count() const
	{
		return count;
	}

	double get_average() const
	{
		return total / ( count ? count : 1);
	}

	const char *get_name() const
	{
		return name;
	}

	const char *get_function() const
	{
		return func;
	}
};

std::ostream& operator<<(std::ostream &os, const CPUClock &clock);

char* strip_types(const char* pretty_func);

#define DEFINE_CPU_CLOCK(var) CPUClock var = CPUClock(#var, strip_types(__PRETTY_FUNCTION__))

#endif
