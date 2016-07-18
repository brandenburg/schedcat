#include <stdint.h>
#include <cassert>
#include <climits>
#include <cmath>

#include "linprog/varmapperbase.h"
#include "linprog/solver.h"

#include "sharedres_types.h"

#include "iter-helper.h"
#include "stl-helper.h"
#include "stl-io-helper.h"
#include "math-helper.h"

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

#include "lp_pedf_analysis.h"

// ------------------------------------------------------------------
// --------------------[ A N A L Y S I S ]---------------------------
// ------------------------------------------------------------------

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_TIMEOUT__

#include <signal.h>
#include <unistd.h>

bool timeout_fired = false;
const unsigned TIMEOUT = 60; // seconds
#endif

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_HP_STOP__
unsigned long gcd(unsigned long a, unsigned long b)
{
	while(true)
	{
		if (a == 0)
			return b;
		b %= a;

		if (b == 0)
			return a;
		a %= b;
	}
}

unsigned long lcm(unsigned long a, unsigned long b)
{
	unsigned long temp = gcd(a, b);
	return temp ? (a / temp * b) : 0;
}
#endif

PEDFBlockingAnalysis::PEDFBlockingAnalysis(const ResourceSharingInfo& _info, unsigned int _cluster) :
	info(_info), cluster(_cluster)
{
	max_deadline = 0;

	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	max_deadline = (max_deadline > T_i->get_deadline() ? max_deadline : T_i->get_deadline());

	min_deadline = max_deadline;

	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	min_deadline = (T_i->get_deadline() < min_deadline ? T_i->get_deadline() : min_deadline);
}

unsigned long PEDFBlockingAnalysis::DBF(unsigned long interval_length)
{
	unsigned long retval = 0;
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	retval += T_i->get_pedf_PDC_max_num_local_jobs(interval_length) * T_i->get_cost();

	return retval;
}

unsigned long PEDFBlockingAnalysis::arrival_curve(unsigned long interval_length)
{
	unsigned long retval = 0;
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	retval += T_i->get_pedf_AC_max_num_local_jobs(interval_length) * T_i->get_cost();

	return retval;
}

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_TIMEOUT__
void watchdog_handler(int sig)
{
	timeout_fired = true;
}
#endif

bool PEDFBlockingAnalysis::is_schedulable()
{

	unsigned long lastBW_Len = 1;

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_HP_STOP__
	unsigned long hyper_period = 1;

	foreach(info.get_tasks(), T_i)
	{
		hyper_period = lcm(hyper_period, T_i->get_period());
	}
#endif

#if defined(__DEBUG_PEDF_BLK_ANALYSIS__) && defined(__PEDF_BLK_ANALYSIS_ENABLE_HP_STOP__)
	std::cout << "[PEDF-BLK] Hyper-period = " << hyper_period << std::endl;
#endif

	unsigned long blk_LB_in = 0, blk_LB_out = 0;

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_TIMEOUT__

	timeout_fired = false;

	// Setup for the timeout
	signal(SIGALRM, watchdog_handler);
	alarm(TIMEOUT);

	while (!timeout_fired)
#else
	while (true)
#endif
		// Perform PDC until the first idle-time
	{
		// Fixed-point iteration step
		unsigned long newBW_Len = arrival_curve(lastBW_Len) + compute_blocking_AC(lastBW_Len);

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[PEDF-BLK] ARRCRV Update : lastBW_Len = " << lastBW_Len << ", newBW_Len=" << newBW_Len << std::endl;
#endif

		if (newBW_Len == lastBW_Len)
			break;

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_HP_STOP__
		if (newBW_Len > hyper_period)
			return false;
#endif

		const unsigned long t_LB = (lastBW_Len > min_deadline) ? lastBW_Len : min_deadline;

		//if (!raw_PDC(t_LB, newBW_Len))
		if (!QPA(t_LB, newBW_Len, blk_LB_in, blk_LB_out))
			return false;

		blk_LB_in = blk_LB_out;

		lastBW_Len = newBW_Len;
	}

#ifdef __PEDF_BLK_ANALYSIS_ENABLE_TIMEOUT__
	if (timeout_fired)
	{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[PEDF-BLK] Timeout Fired" << std::endl;
#endif

		return false;
	}
#endif

	return true;
}

// Compute the last check-point < interval_length for the PDC
unsigned long PEDFBlockingAnalysis::last_check_point_before(unsigned long interval_length)
{
	unsigned long last_check_point = 0;

	foreach(info.get_tasks(), T_i)
	{
		// Local tasks
		if (T_i->get_cluster() == cluster && T_i->get_deadline() < interval_length)
		{
			// steps of nljobs(T_i,t)
			unsigned long d = divide_with_floor(interval_length - T_i->get_deadline(), T_i->get_period()) *
			                  T_i->get_period() + T_i->get_deadline();
			if (d == interval_length)
				d = d - T_i->get_period();
			if (d > last_check_point)
				last_check_point = d;

			// steps of ceil(T_i,t)
			const unsigned long njobs = divide_with_ceil(interval_length, T_i->get_period());
			d = (njobs - 1) * T_i->get_period() + 1;
			if (d == interval_length)
				d = d - T_i->get_period();
			if (d > last_check_point)
				last_check_point = d;
		}

		// Remote tasks
		if (T_i->get_cluster() != cluster)
		{
			// steps of nrjobs(T_i,t)
			const unsigned long njobs = divide_with_ceil(interval_length + T_i->get_deadline(), T_i->get_period());
			if (njobs > 1)
			{
				unsigned long d = (njobs - 1) * T_i->get_period() - T_i->get_deadline() + 1;
				if (d == interval_length)
					d = d - T_i->get_period();
				if (d > last_check_point)
					last_check_point = d;
			}
		}
	}

	return last_check_point;
}

// [QPA] --- Smart implementation of the PDC
// Zhang and Burns - "Schedulability Analysis for Real-Time Systems with EDF Scheduling"
// IEEE Transaction of Computers, 2009
// -------------------------------------------------------------------------------------
// Checks the PDC in [t_LB, t_UB)
//bool PEDFBlockingAnalysis::QPA(unsigned long t_LB, unsigned long t_UB)
bool PEDFBlockingAnalysis::QPA(unsigned long t_LB, unsigned long t_UB, unsigned long blk_LB_in, unsigned long& blk_LB_out)
{
	unsigned long check_point = last_check_point_before(t_UB);

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
	std::cout << "[QPA] First Check-Point = " << check_point << std::endl;
#endif

	unsigned long total_demand = 0;
	blk_LB_out = blk_LB_in;
	bool found_blk_LB = false;

	while (true)
	{
		if (check_point < t_LB)
			break;

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[QPA] Checking t = " << check_point << std::endl;
#endif

		// First compute a coarse-grain upper-bound with integer relaxation
		unsigned long blk = compute_blocking_PDC(check_point);
		total_demand = DBF(check_point) + blk;

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[QPA] Blk UB = " << blk << " total_demand = " << total_demand << std::endl;
#endif

		if (total_demand < t_LB)
			break;
		if (total_demand > check_point)
		{
			// Compute the actual upper-bound without integer relaxation
			blk = compute_tighter_blocking_PDC(check_point, blk, blk_LB_in);
			total_demand = DBF(check_point) + blk;

			if (!found_blk_LB)
			{
				blk_LB_out = blk;
				found_blk_LB = true;
			}

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
			std::cout << "[QPA] Blk Tight = " << blk << " total_demand = " << total_demand << std::endl;
#endif

			if (total_demand > check_point)
			{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
				std::cout << "[QPA] Deadline miss at t = " << check_point << std::endl;
#endif
				return false;
			}
		}

		// Move to the next check-point
		if (total_demand <  check_point)
			check_point = total_demand;
		else // total_demand == check_point
			check_point = last_check_point_before(check_point);
	}

	return true;
}

// Dumb implementation of the PDC
bool PEDFBlockingAnalysis::raw_PDC(unsigned long t_LB, unsigned long t_UB)
{
	//foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	foreach(info.get_tasks(), T_i)
	{
		unsigned long check_point;
		unsigned long nJobs = -1;
		unsigned long total_demand = 0;

		while (true)
		{
			nJobs++;
			// Local tasks
			if (T_i->get_cluster() == cluster)
			{
				check_point = T_i->get_deadline() + nJobs * T_i->get_period();
				const unsigned long check_point2 = (nJobs + 1) * T_i->get_period() + 1;
				check_point = check_point>check_point2 ? check_point : check_point2;
			}
			// Remote tasks
			else
				check_point = (nJobs + 1) * T_i->get_period() - T_i->get_deadline() + 1;

			if (check_point < t_LB)
				continue;
			if (check_point > t_UB)
				break;


#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
			std::cout << "[DUMB-PDC] Checking t = " << check_point << std::endl;
#endif

			// First compute a coarse-grain upper-bound with integer relaxation
			unsigned long blk = compute_blocking_PDC(check_point);
			total_demand = DBF(check_point) + blk;

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
			std::cout << "[DUMB-PDC] Blk UB = " << blk << " total_demand = " << total_demand << std::endl;
#endif

			if (total_demand > check_point)
			{
				// Compute the actual upper-bound without integer relaxation
				blk = compute_tighter_blocking_PDC(check_point, blk);
				total_demand = DBF(check_point) + blk;

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
				std::cout << "[DUMB-PDC] Blk Tight = " << blk << " total_demand = " << total_demand << std::endl;
#endif

				if (total_demand > check_point)
				{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
					std::cout << "[DUMB-PDC] Deadline miss at t = " << check_point << std::endl;
#endif
					return false;
				}
			}
		}
	}

	return true;
}