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

#include "lp_pedf_spinlocks_common.h"
#include "lp_pedf_analysis.h"


class FIFO_Preemptive : public PEDFBlockingAnalysisLP_Spinlocks
{
private:

	void add_no_transitive_arrival_blocking();
	void add_max_number_of_cancellations();
	void add_max_overall_number_of_preemptions();
	void add_at_max_one_request_per_processor_spin();
	void add_per_task_bound_spin_delay();
	void add_blocking_lower_and_upper_bound(unsigned long blocking_LB,
	                                        unsigned long blocking_UB);

	bool integer_relaxation;

public:
	FIFO_Preemptive(const ResourceSharingInfo& info,
	                analysis_type_t analysis_type,
	                unsigned long interval_length,
	                unsigned int cluster,
	                unsigned long blocking_LB,
	                unsigned long blocking_UB = 0, //Default: no UB
	                bool relax = true)
		: PEDFBlockingAnalysisLP_Spinlocks(info, analysis_type, interval_length, cluster)
	{
		integer_relaxation = relax;

		// Add specific constraints for FIFO Preemptive spin locks
		add_no_transitive_arrival_blocking();
		add_max_number_of_cancellations();
		add_max_overall_number_of_preemptions();
		add_at_max_one_request_per_processor_spin();

		add_blocking_lower_and_upper_bound(blocking_LB, blocking_UB);

		vars.seal(); // every possible variable should have been referenced
	}
};

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------

// Constraint 24: No trnasitive arrival blocking
void FIFO_Preemptive::add_no_transitive_arrival_blocking()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
	{
		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;
			const unsigned int x = T_x->get_id();
			var_t X_ARRIVAL = vars.arrival(x, q);

			exp->add_var(X_ARRIVAL);
		}
	}
	add_inequality(exp,0);
}

// Constraint 25: Upper-bound on maximum number of cancellations based on
//                maximum number of preemptions and exclude cancellations
//                when a task is not accessing a resource
void FIFO_Preemptive::add_max_number_of_cancellations()
{

	LinearExpression *exp_no_canc = new LinearExpression();

	// For each local task T_i
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		LinearExpression *exp = new LinearExpression();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;
			var_t C_iq = vars.cancellations(i, q);

			declare_variable_bounds(C_iq, true, 0, false, 0);

			if (!integer_relaxation)
				declare_variable_integer(C_iq);

			exp->add_var(C_iq);

			// Collects C_iq for tasks non accessing a resource
			if (T_i->get_num_requests(q) == 0)
				exp_no_canc->add_var(C_iq);
		}

		unsigned int RHS = 0;
		foreach_task_in_cluster_having_lt_dline(info.get_tasks(), cluster, T_i->get_deadline(), T_h)
		{
			// Compute Upper-Bound on the maximum number of preemptions on T_i by T_h
			const long dline_diff = (long)T_i->get_deadline() - (long)T_h->get_deadline();
			const unsigned int preempt_UB = (dline_diff < 0) ? 0 : divide_with_ceil(dline_diff, T_h->get_period());

			RHS += preempt_UB;
		}

		unsigned long njobs = 0;

		if (lp_type == PDC_MODE)
			njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
		if (lp_type == AC_MODE)
			njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

		// Enforce UB
		add_inequality(exp, RHS * njobs);
	}

	// Enforce no cancellations when a task is not accessing a resource
	add_inequality(exp_no_canc,0);
}

// Constraint 26: Overall number of preemptions bounded by max number of releases
void FIFO_Preemptive::add_max_overall_number_of_preemptions()
{
	// For each local task T_i
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		LinearExpression *exp = new LinearExpression();

		// For each local task T_j having d_j <= d_i
		foreach_task_in_cluster_having_leq_dline(info.get_tasks(), cluster, T_i->get_deadline(), T_j)
		{
			const unsigned int j = T_j->get_id();

			foreach(all_resources,q_iter)
			{
				const unsigned int q = *q_iter;
				var_t C_jq = vars.cancellations(j, q);

				exp->add_var(C_jq);
			}
		}

		unsigned long RHS = 0;
		foreach_task_in_cluster_having_lt_dline(info.get_tasks(), cluster, T_i->get_deadline(), T_x)
		{
			RHS += divide_with_ceil(interval_length, T_x->get_period());
		}

		add_inequality(exp,RHS);
	}

}

// Constraint 27: At max one request per remote processor for each actual local request
//                (include cancellations)
void FIFO_Preemptive::add_at_max_one_request_per_processor_spin()
{
	// For each processor k, different from the one under observation
	foreach_cluster_except(info, cluster, k)
	{
		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			LinearExpression *exp = new LinearExpression();
			unsigned long RHS = 0;

			// For each task T_x in processor k
			foreach_task_in_cluster(info.get_tasks(), k, T_x)
			{
				const unsigned int x = T_x->get_id();
				var_t X_SPIN    = vars.spin(x, q);
				exp->add_var(X_SPIN);
			}

			// For each task T_i in the processor under observation
			foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
			{
				const unsigned int i = T_i->get_id();
				unsigned long njobs = 0;

				if (lp_type == PDC_MODE)
					njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
				if (lp_type == AC_MODE)
					njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

				RHS += njobs * T_i->get_num_requests(q);

				var_t C_iq = vars.cancellations(i, q);
				exp->sub_var(C_iq);
			}

			add_inequality(exp,RHS);
		}
	}
}

// Extra constraint for speed-up
void FIFO_Preemptive::add_blocking_lower_and_upper_bound(unsigned long blocking_LB,
        unsigned long blocking_UB)
{
	LinearExpression *obj_minus = new LinearExpression();
	LinearExpression *obj_plus  = new LinearExpression();

	foreach(taskset, T_x)
	{
		const unsigned int x = T_x->get_id();

		foreach(all_resources, q_iter)
		{
			const unsigned int q = *q_iter;
			const double length = T_x->get_request_length(q);

			var_t X_SPIN    = vars.spin(x, q);
			var_t X_ARRIVAL = vars.arrival(x, q);

			if (length > 0)
			{
				obj_minus->sub_term(length, X_SPIN);
				obj_minus->sub_term(length, X_ARRIVAL);

				obj_plus->add_term(length, X_SPIN);
				obj_plus->add_term(length, X_ARRIVAL);
			}
		}
	}

	const double lb = (double)blocking_LB - 1.0 > 0 ? 0.0 : (double)blocking_LB - 1.0;

	// obj >= LB  ==>  LB <= obj  ==>  LB - obj <= 0  ==> -obj <= -LB
	add_inequality(obj_minus, -lb);

	if (blocking_UB > 0)
		add_inequality(obj_plus, blocking_UB);
}

// ------------------------------------------------------------------
//--------------[ B L O C K I N G     M E T H O D S ]-----------------
// ------------------------------------------------------------------

class PEDFBlockingAnalysisFIFO_Preemptive : public PEDFBlockingAnalysis
{

private:
	unsigned long compute_blocking_PDC(unsigned long interval_length);
	unsigned long compute_blocking_AC (unsigned long interval_length);
	unsigned long compute_tighter_blocking_PDC(unsigned long interval_length,
	        unsigned long blk_UB,
	        unsigned long blk_LB = 0);

	unsigned long ac_blocking_LB;

public:
	PEDFBlockingAnalysisFIFO_Preemptive(const ResourceSharingInfo& info,
	                                    unsigned int cluster)
		: PEDFBlockingAnalysis(info, cluster)
	{
		ac_blocking_LB  = 0;
	}

};

unsigned long PEDFBlockingAnalysisFIFO_Preemptive::compute_blocking_PDC(unsigned long interval_length)
{
	FIFO_Preemptive mip(info, PDC_MODE, interval_length, cluster, 0);

	return mip.solve(false);
}

// No integer relaxation
unsigned long PEDFBlockingAnalysisFIFO_Preemptive::compute_tighter_blocking_PDC(
    unsigned long interval_length,
    unsigned long blk_UB,
    unsigned long blk_LB)
{
	unsigned long pdc_blocking_LB = blk_LB;

	// EDF arrival blocking is not monotonic before max_deadline
	if (interval_length <= max_deadline)
		pdc_blocking_LB = 0;

	FIFO_Preemptive mip(info, PDC_MODE, interval_length, cluster, pdc_blocking_LB, blk_UB, false);

	return mip.solve(false);
}

unsigned long PEDFBlockingAnalysisFIFO_Preemptive::compute_blocking_AC (unsigned long interval_length)
{
	FIFO_Preemptive mip(info, AC_MODE, interval_length, cluster, ac_blocking_LB, 0, false);

	ac_blocking_LB = mip.solve(false);

#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
	std::cout << "[FIFO-P] BLK AC = " << ac_blocking_LB << std::endl;
#endif

	return ac_blocking_LB;
}

// ------------------------------------------------------------------
// --------------------[ E N T R Y    P O I N T ]--------------------
// ------------------------------------------------------------------

bool lp_pedf_fifo_preempt_is_schedulable(const ResourceSharingInfo& info)
{
	foreach_cluster(info, k)
	{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[FIFO-P] CPU#" << k << std::endl;
#endif

		// Perform schedulability analysis for each processor k
		PEDFBlockingAnalysisFIFO_Preemptive analysis(info, k);
		if (!analysis.is_schedulable())
			return false;
	}

	return true;
}