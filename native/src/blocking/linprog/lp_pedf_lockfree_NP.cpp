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

class LockFree_NP : public PEDFBlockingAnalysisLP_LockFree
{
private:

	// Constraints
	void add_arrival_blocking_max_one_local_commit();
	void add_no_arrival_blocking_dline_inside_interval();
	void add_no_commit_no_arrival_blocking();
	void add_transitive_arrival_blocking_max_one_local_commit();
	void add_no_local_conflicts();
	void add_rta_based_bound_on_remote_conflicts();

	// Utility methods
	unsigned long compute_NP_commit_response_time(taskIterator T_i, const unsigned int q);

public:
	LockFree_NP(const ResourceSharingInfo& info,
	            analysis_type_t analysis_type,
	            unsigned long interval_length,
	            unsigned int cluster,
	            unsigned long blocking_LB,
	            unsigned long blocking_UB = 0, //Default: no UB
	            bool relax = true)
		: PEDFBlockingAnalysisLP_LockFree(info, analysis_type, interval_length, cluster,
		                                  blocking_LB, blocking_UB, relax)
	{

		add_arrival_blocking_max_one_local_commit();
		add_no_arrival_blocking_dline_inside_interval();
		add_no_commit_no_arrival_blocking();
		add_transitive_arrival_blocking_max_one_local_commit();
		add_no_local_conflicts();
		add_rta_based_bound_on_remote_conflicts();

		vars.seal(); // every possible variable should have been referenced
	}
};

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------

// Arrival blocking can be caused by at most one local commit
void LockFree_NP::add_arrival_blocking_max_one_local_commit()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			var_t A_i_q = vars.indicator_arrival(i,q);

			exp->add_var(A_i_q);
		}
	}
	add_inequality(exp, 1);
}

// Arrival blocking cannot be caused by commits of tasks having dline <= t
void LockFree_NP::add_no_arrival_blocking_dline_inside_interval()
{
	LinearExpression *exp = new LinearExpression();

	foreach_task_in_cluster_having_leq_dline(taskset, cluster, interval_length, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(T_i->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			var_t A_i_q = vars.indicator_arrival(i,q);

			exp->add_var(A_i_q);
		}
	}

	add_inequality(exp, 0);
}

// Arrival blocking on a resource q cannot be caused by tasks that have
// no commit loops on q
void LockFree_NP::add_no_commit_no_arrival_blocking()
{
	foreach_task_in_cluster_having_leq_dline(taskset, cluster, interval_length, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(T_i->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			var_t A_i_q = vars.indicator_arrival(i,q);

			LinearExpression *exp = new LinearExpression();
			exp->add_var(A_i_q);

			// A_{i,q} <= N_{i,q}
			add_inequality(exp, T_i->get_num_requests(q));
		}
	}
}

// Transitive arrival blocking can affect at most one local commit
void LockFree_NP::add_transitive_arrival_blocking_max_one_local_commit()
{
	unsigned long bigM = 0;

	foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
	foreach(all_resources,q_iter)
		bigM += T_x->get_pedf_max_num_remote_jobs(interval_length) * T_x->get_num_requests(*q_iter);

	unsigned long njobs = 0;

	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{

		if (lp_type == PDC_MODE)
			njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
		if (lp_type == AC_MODE)
			njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

		if (njobs > 0)
			continue;

		// Here njobs=0

		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;
			var_t Y_R_i_q = vars.remote_conflicts (i,q);
			var_t A_i_q   = vars.indicator_arrival(i,q);

			LinearExpression *exp = new LinearExpression();
			exp->add_var(Y_R_i_q);

			exp->sub_term(bigM,A_i_q);

			// Y_R_i_q <= A_i_q*bigM  ==>  Y_R_i_q - A_i_q*bigM <= 0
			add_inequality(exp,0);
		}
	}
}

// Under Non-Preemptive commit loops, there are no local conflicts
void LockFree_NP::add_no_local_conflicts()
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

				var_t Y_L_i_j_q = vars.local_conflicts(i,j,q);

				exp->add_var(Y_L_i_j_q);
			}
		}
	}

	add_inequality(exp, 0);
}

unsigned long LockFree_NP::compute_NP_commit_response_time(taskIterator T_i, const unsigned int q)
{
	// We compute the response-time for completing a commit on
	// resource q issued by task T_i

	unsigned long W     = T_i->get_request_length(q);
	unsigned long W_new;

	while (true)
	{
		W_new = T_i->get_request_length(q);

		foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
		{
			W_new += T_x->get_pedf_max_num_remote_jobs(W) * T_x->get_num_requests(q) *
			         T_i->get_request_length(q);
		}

		if (W == W_new || W_new > T_i->get_deadline())
			break;

		W = W_new;
	}

	return W_new;
}

void LockFree_NP::add_rta_based_bound_on_remote_conflicts()
{

	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			unsigned long W = compute_NP_commit_response_time(T_i, q);

			// Do not insert the constraint for T_i accessing resource q
			if (W > T_i->get_deadline())
				continue;

			// Here W <= d_i

			var_t Y_R_i_q = vars.remote_conflicts(i,q);

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

class PEDFBlockingAnalysisLockFree_NP : public PEDFBlockingAnalysis
{

private:
	unsigned long compute_blocking_PDC(unsigned long interval_length);
	unsigned long compute_blocking_AC (unsigned long interval_length);
	unsigned long compute_tighter_blocking_PDC(unsigned long interval_length,
	        unsigned long blk_UB,
	        unsigned long blk_LB = 0);

	unsigned long ac_blocking_LB;

public:
	PEDFBlockingAnalysisLockFree_NP(const ResourceSharingInfo& info,
	                                unsigned int cluster)
		: PEDFBlockingAnalysis(info, cluster)
	{
		ac_blocking_LB  = 0;
	}

};

unsigned long PEDFBlockingAnalysisLockFree_NP::compute_blocking_PDC(unsigned long interval_length)
{

	LockFree_NP mip(info, PDC_MODE, interval_length, cluster, 0);

	return mip.solve(false);
}

// No integer relaxation
unsigned long PEDFBlockingAnalysisLockFree_NP::compute_tighter_blocking_PDC(
    unsigned long interval_length,
    unsigned long blk_UB,
    unsigned long blk_LB)
{
	unsigned long pdc_blocking_LB = blk_LB;

	// EDF arrival blocking is not monotonic before max_deadline
	if (interval_length <= max_deadline)
		pdc_blocking_LB = 0;

	LockFree_NP mip(info, PDC_MODE, interval_length, cluster, pdc_blocking_LB, blk_UB, false);

	pdc_blocking_LB = mip.solve(false);
	return pdc_blocking_LB;
}

unsigned long PEDFBlockingAnalysisLockFree_NP::compute_blocking_AC (unsigned long interval_length)
{
	LockFree_NP mip(info, AC_MODE, interval_length, cluster, ac_blocking_LB, 0, false);

	ac_blocking_LB = mip.solve(false);
	return ac_blocking_LB;
}

// ------------------------------------------------------------------
// --------------------[ E N T R Y    P O I N T ]--------------------
// ------------------------------------------------------------------

bool lp_pedf_lockfree_NP_is_schedulable(const ResourceSharingInfo& info)
{
	foreach_cluster(info, k)
	{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[LF-NP] CPU#" << k << std::endl;
#endif

		// Perform schedulability analysis for each processor k
		PEDFBlockingAnalysisLockFree_NP analysis(info, k);
		if (!analysis.is_schedulable())
			return false;
	}

	return true;
}