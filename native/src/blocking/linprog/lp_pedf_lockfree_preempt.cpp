#include <stdint.h>
#include <cassert>
#include <cmath>
#include <climits>

#include "linprog/model.h"
#include "linprog/varmapperbase.h"
#include "linprog/solver.h"

#include "sharedres_types.h"

#include "iter-helper.h"
#include "stl-helper.h"
#include "stl-io-helper.h"

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

#include "lp_pedf_lockfree_common.h"
#include "lp_pedf_analysis.h"

#define taskIterator TaskInfos::const_iterator

class LockFree_Preemptive : public PEDFBlockingAnalysisLP_LockFree
{
private:

	// Constraints
	void add_no_jobs_no_retry_delay();
	void add_no_requests_no_cause_local_conflict();
	void add_at_most_one_retry_per_preempting_job();
	void add_each_job_causes_at_most_one_retry_per_resource();
	void add_rta_based_bound_on_remote_conflicts();

	// Utility methods

	unsigned long compute_preemptive_commit_response_time(taskIterator T_i,
	        const unsigned int q);

	unsigned long get_max_commit_length(const unsigned int k, taskIterator T_i,
	                                    const unsigned int q, unsigned long t);

	unsigned long get_effective_demand(TaskInfos::const_iterator T_h, taskIterator T_i,
	                                   const unsigned int q);


public:
	LockFree_Preemptive(const ResourceSharingInfo& info,
	                    analysis_type_t analysis_type,
	                    unsigned long interval_length,
	                    unsigned int cluster,
	                    unsigned long blocking_LB,
	                    unsigned long blocking_UB = 0, //Default: no UB
	                    bool relax = true)
		: PEDFBlockingAnalysisLP_LockFree(info, analysis_type, interval_length, cluster,
		                                  blocking_LB, blocking_UB, relax)
	{
		// Available from PEDFBlockingAnalysisLP_LockFree
		add_no_arrival_blocking();

		add_no_jobs_no_retry_delay();
		add_no_requests_no_cause_local_conflict();
		add_at_most_one_retry_per_preempting_job();
		add_each_job_causes_at_most_one_retry_per_resource();
		add_rta_based_bound_on_remote_conflicts();

		vars.seal(); // every possible variable should have been referenced
	}
};

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------

// Enforce no retry delay for tasks that have no jobs in a deadline busy-period
void LockFree_Preemptive::add_no_jobs_no_retry_delay()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		unsigned long njobs = 0;

		const unsigned int i = T_i->get_id();

		if (lp_type == PDC_MODE)
			njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
		if (lp_type == AC_MODE)
			njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);


		if (njobs > 0)
			continue;

		// Here njobs=0

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			var_t Y_R_i_q = vars.remote_conflicts (i,q);
			exp->add_var(Y_R_i_q);

			foreach_task_in_cluster(info.get_tasks(), cluster, T_j)
			{
				const unsigned int j = T_j->get_id();
				var_t Y_L_i_j_q = vars.local_conflicts (i,j,q);
				exp->add_var(Y_L_i_j_q);
			}
		}
	}

	add_inequality(exp,0);
}

// Tasks that do not have commit loops on a resource cannot cause conflict
// for such a resource
void LockFree_Preemptive::add_no_requests_no_cause_local_conflict()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			foreach_task_in_cluster(info.get_tasks(), cluster, T_j)
			{
				const unsigned int j = T_j->get_id();

				if (T_j->get_num_requests(q) > 0)
					continue;

				// Here N_{j,q}=0

				var_t Y_L_i_j_q = vars.local_conflicts (i,j,q);
				exp->add_var(Y_L_i_j_q);
			}
		}
	}

	add_inequality(exp,0);
}

// "One-to-one mapping" between retries in a task and preempting jobs
void LockFree_Preemptive::add_at_most_one_retry_per_preempting_job()
{
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach_task_in_cluster(info.get_tasks(), cluster, T_j)
		{
			const unsigned int j = T_j->get_id();

			// LHS
			LinearExpression *exp = new LinearExpression();
			foreach(all_resources,q_iter)
			{
				const unsigned int q = *q_iter;
				var_t Y_L_i_j_q = vars.local_conflicts (i,j,q);
				exp->add_var(Y_L_i_j_q);
			}

			// RHS

			// Compute Upper-Bound on the maximum number of preemptions on T_i by T_j
			const long dline_diff = (long)T_i->get_deadline() - (long)T_j->get_deadline();
			const unsigned long preempt_UB = (dline_diff < 0) ? 0 :
			                                 divide_with_ceil(dline_diff,T_j->get_period());

			unsigned long njobs = 0;
			if (lp_type == PDC_MODE)
				njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
			if (lp_type == AC_MODE)
				njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

			add_inequality(exp, preempt_UB * njobs);
		}
	}
}

// Each job can cause at most one retry for each resource
void LockFree_Preemptive::add_each_job_causes_at_most_one_retry_per_resource()
{
	foreach_task_in_cluster(info.get_tasks(), cluster, T_j)
	{
		const unsigned int j = T_j->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			LinearExpression *exp = new LinearExpression();
			foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
			{
				const unsigned int i = T_i->get_id();
				var_t Y_L_i_j_q = vars.local_conflicts (i,j,q);
				exp->add_var(Y_L_i_j_q);
			}

			add_inequality(exp, divide_with_ceil(interval_length,T_j->get_period()));
		}
	}
}

// Returns the maximum length for resource k among the commits for which a task with
// deadline > t can cause retry while a commit on q by T_i is pending
unsigned long LockFree_Preemptive::get_max_commit_length(const unsigned int k, taskIterator T_i,
        const unsigned int q, unsigned long t)
{
	unsigned long retval = 0;

	// For every task T_x having t < d_x < d_i, compute the max L_{x,k}
	foreach_task_in_cluster(info.get_tasks(), cluster, T_x)
	{
		if (t < T_x->get_deadline() && T_x->get_deadline() < T_i->get_deadline())
			retval = (T_x->get_request_length(q) > retval) ? T_x->get_request_length(q) : retval;
	}

	// If k==q, consider also L_{i,q=k}
	if (k == q)
		retval = (T_i->get_request_length(q) > retval) ? T_i->get_request_length(q) : retval;

	return retval;
}

// Returns the effective demand generated by each job of task T_h, that is
// its execution cost plus the maximum retry delay that can generate
unsigned long LockFree_Preemptive::get_effective_demand(taskIterator T_h, taskIterator T_i,
        const unsigned int q)
{
	unsigned long retval = T_h->get_cost();

	foreach(all_resources,k_iter)
	{
		const unsigned int k = *k_iter;

		if (T_h->get_num_requests(k) > 0)
			retval += get_max_commit_length(k, T_i, q, T_h->get_deadline());
	}

	return retval;
}

unsigned long LockFree_Preemptive::compute_preemptive_commit_response_time(
    taskIterator T_i, const unsigned int q)
{
	// We compute the response-time for completing a commit on
	// resource q issued by task T_i

	unsigned long W     = T_i->get_request_length(q);
	unsigned long W_new;

	while (true)
	{
		W_new = T_i->get_request_length(q);

		foreach_task_in_cluster_having_lt_dline(info.get_tasks(), cluster, T_i->get_deadline(), T_h)
		{
			long dline_difference = (long)T_i->get_deadline() - (long)T_h->get_deadline();
			dline_difference = (dline_difference < 0) ? 0 : dline_difference;

			unsigned long minval = ((unsigned) dline_difference < W) ? dline_difference : W;

			W_new += divide_with_ceil(minval,T_h->get_period()) * get_effective_demand(T_h,T_i,q);
		}

		foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
		foreach(all_resources,k_iter)
			W_new += T_x->get_pedf_max_num_remote_jobs(W) * T_x->get_num_requests(*k_iter) *
			         get_max_commit_length(*k_iter, T_i, q, 0);

		if (W == W_new || W_new > T_i->get_deadline())
			break;

		W = W_new;
	}

	return W_new;
}

void LockFree_Preemptive::add_rta_based_bound_on_remote_conflicts()
{

	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			unsigned long W = compute_preemptive_commit_response_time(T_i, q);

			// Do not insert the constraint for T_i accessing resource q
			if (W > T_i->get_deadline())
				continue;

			// Here W <= d_i

			var_t Y_R_i_q = vars.remote_conflicts (i,q);

			unsigned long njobs = 0;

			if (lp_type == PDC_MODE)
				njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
			if (lp_type == AC_MODE)
				njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

			unsigned long RHS = 0;
			foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
			{
				RHS += T_x->get_pedf_max_num_remote_jobs(W) * T_x->get_num_requests(q) *
				       njobs * T_i->get_num_requests(q);
			}

			LinearExpression *exp = new LinearExpression();
			exp->add_var(Y_R_i_q);

			add_inequality(exp, RHS);
		}
	}
}

// ------------------------------------------------------------------
//--------------[ B L O C K I N G     M E T H O D S ]-----------------
// ------------------------------------------------------------------

class PEDFBlockingAnalysisLockFree_Preemptive : public PEDFBlockingAnalysis
{

private:
	unsigned long compute_blocking_PDC(unsigned long interval_length);
	unsigned long compute_blocking_AC (unsigned long interval_length);
	unsigned long compute_tighter_blocking_PDC(unsigned long interval_length,
	        unsigned long blk_UB,
	        unsigned long blk_LB = 0);

	unsigned long ac_blocking_LB;

public:
	PEDFBlockingAnalysisLockFree_Preemptive(const ResourceSharingInfo& info,
	                                        unsigned int cluster)
		: PEDFBlockingAnalysis(info, cluster)
	{
		ac_blocking_LB  = 0;
	}
};

unsigned long PEDFBlockingAnalysisLockFree_Preemptive::compute_blocking_PDC(unsigned long interval_length)
{
	LockFree_Preemptive mip(info, PDC_MODE, interval_length, cluster, 0);

	return mip.solve(false);
}

// No integer relaxation
unsigned long PEDFBlockingAnalysisLockFree_Preemptive::compute_tighter_blocking_PDC(
    unsigned long interval_length,
    unsigned long blk_UB,
    unsigned long blk_LB)
{
	unsigned long pdc_blocking_LB = blk_LB;

	// EDF arrival blocking is not monotonic before max_deadline
	if (interval_length <= max_deadline)
		pdc_blocking_LB = 0;

	LockFree_Preemptive mip(info, PDC_MODE, interval_length, cluster, pdc_blocking_LB, blk_UB, false);

	return mip.solve(false);
}

unsigned long PEDFBlockingAnalysisLockFree_Preemptive::compute_blocking_AC (unsigned long interval_length)
{
	LockFree_Preemptive mip(info, AC_MODE, interval_length, cluster, ac_blocking_LB, 0, false);

	ac_blocking_LB = mip.solve(false);

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
	std::cout << "[LF-P] BLK-AC = " << ac_blocking_LB << std::endl;
#endif

	return ac_blocking_LB;
}

// ------------------------------------------------------------------
// --------------------[ E N T R Y    P O I N T ]--------------------
// ------------------------------------------------------------------

bool lp_pedf_lockfree_preempt_is_schedulable(const ResourceSharingInfo& info)
{
	foreach_cluster(info, k)
	{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[LF-P] CPU#" << k << std::endl;
#endif

		// Perform schedulability analysis for each processor k
		PEDFBlockingAnalysisLockFree_Preemptive analysis(info, k);
		if (!analysis.is_schedulable())
			return false;
	}

	return true;
}