#include "sharedres.h"
#include "blocking.h"
#include "math-helper.h"
#include "stl-helper.h"
#include "iter-helper.h"
#include "global-pip.h"
#include <iostream>
#include <set>

/* Global s-aware analysis of the P-PCP, assuming an (m, n)-configuration:
 *
 *   - for the m highest-priority tasks, alpha_i = n
 *   - for all lower-priority tasks, alpha_i = m
 *
 * Based on Easwaran and Andersson, "Resource Sharing in Global
 * Fixed-Priority Preemptive Multiprocessor Scheduling", RTSS'09.
 */

//Bound the indirect blocking caused by lower-priority tasks
//under the reasonable priority assignment.
//Eq. 16
static unsigned long Ilp_i_ppcp(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk, // task i under analysis
	unsigned int number_of_cpus)
{
	unsigned long R_i = tsk->get_response();
	unsigned long sum = 0, min = UINT_MAX;
	unsigned int num_tasks = info.get_tasks().size();

	std::vector<unsigned int> csl_value(num_tasks + 1, 0);
	std::vector<unsigned int> shift_value(num_tasks + 1, 0);

	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	//Use temporary arrays to save the corresponding
	//values of the lower-priority tasks.
	//The ith item <=> the value of the ith task
	foreach_lower_priority_task(info.get_tasks(), *tsk, tl)
	{
		// The following formulas attempt to capture the intuition
		// laid out in the right-hand column of page 9 of Easwaran and
		// Andersson's paper (page 385 in the proceedings). The paper
		// only provides a rather vague description of how to obtain an
		// appropriate "shift" value. The below bounds and computations
		//  have been deduced by Maolin Yang. His description follows.

		// Calculate the "shift" value:
		// Consider (1) the back-to-back execution of the critical sections
		// of interest (that can block the task, tsk, under analysis) for tl,
		// (2) a job of the task under analysis, Ji, releases when a job of tl
		// starts to execute the critical sections of interest at time t1,
		// (3) the following job of tl finishes its critical secions of
		// interest at time t2. Under the "reasonable priority assignment",
		// at most two jobs of tl can block Ji. Further, the interval
		// between t1 and t2 is L = tl.period - tl.response + 2*csl (csl is the
		// total length of the critical sections of interest). Thus, if the
		// response time of the task under analysis, Ri, is larger than L,
		// then, the release time of Ji can be shifted to a earlier point
		// by Ri - L (it can also be delayed by the two jobs of tl).
		// Analogously, if csl < Ri <= L - csl (at most one job of
		// tl can block Ji), then the release time of Ji can be shifted at most
		// by Ri - csl. Otherwise, shift the release time of Ji to
		// an earlier time will coorespondingly reduce the blocking time
		// caused by jobs of tl.

		// Further details and an illustration may be found in Appendix H.
		// of Yang et al., "Global Real-Time Semaphore Protocols: A Survey,
		// Unified Analysis, and Comparison", 2015.
		// Available at http://www.mpi-sws.org/~bbb/papers/index.html

		unsigned int tl_id = tl->get_id();
		unsigned long csl = lower_priority_with_higher_ceiling_time(info,
			*tsk, *tl, prio_ceilings);
		csl_value[tl_id] = csl;

		if (R_i > tl->get_period() - tl->get_response() + 2 * csl)
			shift_value[tl_id] = R_i + tl->get_response() - tl->get_period() - 2 * csl;
		else if ((R_i > csl) && (R_i <= tl->get_period() - tl->get_response() + csl))
			shift_value[tl_id] = R_i - csl;
		else
			shift_value[tl_id] = 0;

		if((csl != 0)&&(csl < min))
			min = csl;
	}

	unsigned long R_i_prime = R_i - min;

	//Consider the m (if any) smallest "shift" values
	//i.e., the first term of Eq. 16 in the
	//original paper (RTSS'09)
	std::set<unsigned int> considered_tasks;
	for (unsigned int counter = 0; counter < number_of_cpus; counter++)
	{
		min = UINT_MAX;
		bool new_min_found = false;
		unsigned int tl_id;
		foreach_lower_priority_task(info.get_tasks(), *tsk, tl)
		{
			tl_id = tl->get_id();
			if ((considered_tasks.find(tl_id) != considered_tasks.end()) //task not considered yet
			    && (shift_value[tl_id] < min)) //new minimum
			{
				min = shift_value[tl_id];
				new_min_found = true;
				considered_tasks.insert(tl_id);
			}
		}

		if (new_min_found)
			sum += W_l_tx(info, R_i, info.get_tasks().at(tl_id), csl_value[tl_id]);
	}

	//The rest (if any) terms are used for
	//the second terms of Eq. 16
	foreach_lower_priority_task(info.get_tasks(), *tsk, tl)
	{
		unsigned int tl_id = tl->get_id();
		if (considered_tasks.find(tl->get_id()) != considered_tasks.end()) //task not considered yet
				sum += W_l_tx(info, R_i_prime, *tl, csl_value[tl_id]);
	}

	return divide_with_ceil(sum, number_of_cpus);
}


//Summing up the m (if any) longest requests of m
//lower-priority tasks
//Eq. 13: alpha_i = m under the (m,n)-configuration
//sus_ik = sum of the m largest values in {C_lj | l > i cap res_j not res_k}
static unsigned long m_largest_values(
	const ResourceSharingInfo& info,
	const TaskInfo& tsk, // task i under analysis
	unsigned int res_k,
	unsigned int number_of_cpus) // m
{
	std::vector<unsigned int> csls_of_all_LP_tasks;
	unsigned long sum = 0;

	//collect the largest CSL from each LP-task
	foreach_lower_priority_task(info.get_tasks(), tsk, tl)
	{
		unsigned int max_csl_of_tl = 0;
		foreach(tl->get_requests(), req)
		{
			if (req->get_resource_id() != res_k)
				max_csl_of_tl = std::max(max_csl_of_tl, req->get_request_length());
		}
		csls_of_all_LP_tasks.push_back(max_csl_of_tl);
	}

	//sorts in ascending order, so we need the m last values
	std::sort(csls_of_all_LP_tasks.begin(), csls_of_all_LP_tasks.end());

	//sum up the last m last values or whatever is there
	unsigned int values_to_add = csls_of_all_LP_tasks.size();
	if (number_of_cpus < values_to_add)
		values_to_add = number_of_cpus;

	for (unsigned int i = 0; i < values_to_add; i++)
	{
		sum += csls_of_all_LP_tasks.back();
		csls_of_all_LP_tasks.pop_back();
	}

	return sum;
}

//Additional suspensions due to expelling
//Eq. 14: sus_i = sum_{res_k requested by task i} N_ik * sus_ik
static unsigned long sus_i(
	const ResourceSharingInfo& info,
	const TaskInfo& tsk, // task i under analysis
	unsigned int number_of_cpus) // m
{
	unsigned int sum = 0;

	foreach(tsk.get_requests(), request)
	{
		unsigned int res_id = request->get_resource_id();
		unsigned int num_requests = request->get_num_requests();

		sum += num_requests * m_largest_values(info, tsk, res_id, number_of_cpus);
	}
	return sum;
}

//for lower-priority tasks, direct blocking, indirect blocking,
//and suspensions due to expelling are considered as blocking
static unsigned long compute_Ilp_i(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk,
	unsigned int number_of_cpus,
	bool reasonable_priority_assignment)
{
	unsigned long indirect_blocking;

	// If the "reasonable priority assignment" is assumed,
	if (reasonable_priority_assignment)
		indirect_blocking = Ilp_i_ppcp(info, tsk, number_of_cpus);
	else
	// else use the the following for general the other fixed-priority assignment
		indirect_blocking = Ilp_i(info, *tsk, number_of_cpus);

	return indirect_blocking;
}

BlockingBounds* ppcp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus,
	bool reasonable_priority_assignment)
{
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];

		const unsigned long dsr = Ihp_i_dsr(info, &tsk);

		// This is computing RT_i according to Eq. 17.
		// Ihp_i_osr and Ihp_i_nsr are part of the interference considered in the RTA
		// and hence not included here.
		results[i].total_length = DB_i(info, tsk) + dsr;

		// The paper states:
		// "In general, it is always beneficial to set alpha_i = n for the m highest
		// base-priority tasks (i < m). This follows from the fact that Ilp_i =
		// ... = sus_i = 0 for these tasks when alpha_i = n."
		// => we only add sus_i and Ilp_i if i >= m.
		if (tsk.get_priority() >= number_of_cpus)
		{
			results[i].total_length +=
				sus_i(info, tsk, number_of_cpus)
			 	+ compute_Ilp_i(info, &tsk, number_of_cpus,
			 	                reasonable_priority_assignment);
		}

		// We abuse "local" blocking here (which makes no sense under global
		// scheduling) to pass 'dsr' back to the Python wrapper.
		// The response-time analysis code will subtract 'dsr' from
		// the total higher-priority interference bound (since we already
		// account for it in the blocking bound).
		results.set_local_blocking(i, dsr);
	}
	return _results;
}
