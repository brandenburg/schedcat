
#include <sys/time.h>
#include <sys/resource.h>

#include "cpu_time.h"

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

