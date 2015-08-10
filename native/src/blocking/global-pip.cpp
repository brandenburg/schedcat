#include "sharedres.h"
#include "blocking.h"
#include "math-helper.h"
#include "iter-helper.h"

#include "stl-helper.h"

/* Global s-aware analysis of the priority inheritance protocol.
 * Based on Easwaran and Andersson, "Resource Sharing in Global
 * Fixed-Priority Preemptive Multiprocessor Scheduling", RTSS'09.
 */

//calculate the cumulative critical section length of
//task "tx_id" with regard to the resources that will be requested by
//task "tsk"
unsigned long common_sr_time(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk,
	const TaskInfo &tx)
{
	unsigned long sum = 0;

	// check all requests issued by task 'tsk'
	foreach(tsk->get_requests(), request)
	{
		unsigned int res_id = request->get_resource_id();
		foreach(tx.get_requests(), tx_req)
		{
			// sum up
			if (res_id == tx_req->get_resource_id())
				// This corresponds to CT_{i,k}, wrt to tx.
				// "let CT_{i,k} denote the maximum total resource usage time
				// for resource R_k by any single job of T_i (sum of resource
				// usage times over all requests for resource R_k)"
				sum += tx_req->get_request_length() * tx_req->get_num_requests();
		}
	}

	return sum;
}

// Calculate the number of jobs of task 'task_id'
// during an interval of length 'response_time'
// N_l_tx in Eq. 4
static unsigned int N_l_tx(
	const ResourceSharingInfo& info,
	unsigned long t,
	const TaskInfo &task,
	unsigned long x)
{
	// integer division, implicit floor
	return (t + task.get_deadline() - x) / task.get_period();
}

// Calculate the workload of jobs of task 'task',
// considering 'x' time units per job,
// in an interval of length 't'
// W_l(t, x) in Eq. 5
unsigned long W_l_tx(
	const ResourceSharingInfo& info,
	unsigned long t,
	const TaskInfo &task,
	unsigned long x)
{
	unsigned long workload = x * N_l_tx(info, t, task, x); // x*N_l(t,x)
	workload += std::min(x,	// + min(x, ...
							t + task.get_deadline() // ... t + D ...
							- x - task.get_period() * N_l_tx(info, t, task, x)); // ... - x - T*N(t,x))

	return workload;
}

//Bound the blocking (direct blocking) caused by higher-priority tasks.
//The other delays caused by higher-priority tasks are
//considered as interference (thus are not accounted for here)
// Ihp_i_dsr in Eq. 7
unsigned long Ihp_i_dsr(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk)
{
	unsigned long hp_blocking = 0;

	foreach_higher_priority_task(info.get_tasks(), *tsk, th)
	{
		// calculate the cumulative critical section length of interest
		unsigned long csl = common_sr_time(info, tsk, *th);
		// summing up the direct blocking caused by task 'th_id'
		hp_blocking += W_l_tx(info, tsk->get_response(), *th, csl);
	}
	return hp_blocking;
}

//Bound the direct blocking caused by lower-priority tasks.
//Each request can be blocked by at most one lower-priority task
// DB_i, Eq. (6)
unsigned long DB_i(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk)
{
	unsigned long sum = 0;

	foreach(tsk.get_requests(), request)
	{
		unsigned int res_id = request->get_resource_id();
		unsigned int num_requests = request->get_num_requests();
		unsigned long max = 0;

		//find the longest request of resource 'res_id' issued by lower-priority
		//tasks of task 'tsk'
		foreach_lower_priority_task(info.get_tasks(), tsk, tx)
		{
			foreach(tx->get_requests(), req)
			{
				// check if it accesses the resource 'res_id'
				// requested by task 'tsk' and increases the max. CSL
				if ((res_id == req->get_resource_id())
					&& (req->get_request_length() > max))
				{
						max = req->get_request_length();
				}
			}
		}

		//sum up
		sum += max * num_requests;
	}

	return sum;
}

//Calculate the cumulative time that any job of task 'tx_id'
//can use the resources with priority ceiling higher
//than the base priority of task 'tsk'
// sum( ... CT_lx ) in Eq. 10
unsigned long lower_priority_with_higher_ceiling_time(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk,
	const TaskInfo &tx,
	const PriorityCeilings &prio_ceilings)
{
	unsigned long sum = 0;

	// summing up all requests to resources with
	// priority ceiling higher than the base priority
	// of task 'tsk'
	foreach(tx.get_requests(), req)
	{
		unsigned int res_id = req->get_resource_id();

		if (prio_ceilings.at(res_id) < tsk.get_priority())
			sum += req->get_request_length() * req->get_num_requests();
	}

	return sum;
}

//Bound the indirect blocking caused by lower-priority tasks.
//Each request can be blocked by all lower-priority requests
//with higher priority ceilings
// Ilp_i, Eq. (10)
unsigned long Ilp_i(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk,
	unsigned int number_of_cpus)
{
	unsigned long sum = 0;

	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	foreach_lower_priority_task(info.get_tasks(), tsk, tl)
	{
		unsigned long sum_CT_lx = lower_priority_with_higher_ceiling_time(
			info, tsk, *tl, prio_ceilings); // sum( CT_lx )
		sum += W_l_tx(info, tsk.get_response(), *tl, sum_CT_lx); // W_l(RT_i, sum_CT_lx ])
	}

	return divide_with_ceil(sum, number_of_cpus);
}


BlockingBounds* global_pip_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus)
{
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];

		const unsigned long dsr = Ihp_i_dsr(info, &tsk);

		// This is computing RT_i according to Eq. 11.
		// Ihp_i_osr and Ihp_i_nsr are part of the interference considered in the RTA
		// and hence not included here.
		results[i].total_length = DB_i(info, tsk) + dsr;

		// Only add Ilp_i for tasks that are not among the m highest-priority
		// tasks.
		if (tsk.get_priority() >= number_of_cpus)
			results[i].total_length += Ilp_i(info, tsk, number_of_cpus);

		// We abuse "local" blocking here (which makes no sense under global
		// scheduling) to pass 'dsr' back to the Python wrapper.
		// The response-time analysis code will subtract 'dsr' from
		// the total higher-priority interference bound (since we already
		// account for it in the blocking bound).
		results.set_local_blocking(i, dsr);
	}
	return _results;
}
