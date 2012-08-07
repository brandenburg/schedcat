
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <cstring>

#include "cpu_time.h"


#if _POSIX_C_SOURCE >= 199309L

// use clock_xxx() API

double get_cpu_usage(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
	{
		return ts.tv_sec + ts.tv_nsec / 1E9;
	}
	else
		return 0.0;
}


#else

// fall back to getrusage()

#ifdef RUSAGE_THREAD
// This is a Linuxism...
#define ACCOUNTING_SCOPE RUSAGE_THREAD
#else
// This is POSIX.
#define ACCOUNTING_SCOPE RUSAGE_SELF
#endif

double get_cpu_usage(void)
{
	struct rusage u;
	if (getrusage(ACCOUNTING_SCOPE, &u) == 0)
	{
		return u.ru_utime.tv_sec + u.ru_utime.tv_usec / 1E6;
	}
	else
		return 0.0;
}

#endif


std::ostream& operator<<(std::ostream &os, const CPUClock &clock)
{
	if (clock.get_function())
		os << clock.get_function() << "::";
	os << clock.get_name()
	   << ": total=" << clock.get_total() * 1000 << "ms "
	   << "last="    << clock.get_last()  * 1000 << "ms "
	   << "average=" << clock.get_average() * 1000 << "ms "
	   << "count="   << clock.get_count();
	return os;
}

char *strip_types(const char* pretty_func)
{
	char *copy = strdup(pretty_func);

	char *start = strchr(copy, ' ');
	char *end = strchr(copy, '(');

	if (start)
		copy = start + 1;
	if (end)
		*end = '\0';

	return copy;
}
