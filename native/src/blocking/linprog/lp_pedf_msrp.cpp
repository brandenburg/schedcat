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


class MSRP_LP : public PEDFBlockingAnalysisLP_Spinlocks
{
private:

	void add_at_max_one_request_per_processor_spin();
	void add_per_task_bound_spin_delay();
	void add_at_max_one_request_per_processor_arrival();


public:
	MSRP_LP(const ResourceSharingInfo& info,
	        analysis_type_t analysis_type,
	        unsigned long interval_length,
	        unsigned int cluster)
		: PEDFBlockingAnalysisLP_Spinlocks(info, analysis_type, interval_length, cluster)
	{
		// Add specific constraints for MSRP
		add_at_max_one_request_per_processor_spin();
		add_per_task_bound_spin_delay();
		add_at_max_one_request_per_processor_arrival();

		vars.seal(); // every possible variable should have been referenced
	}
};

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------

// Constraint 15: At max one request pe remote processor for each local request
void MSRP_LP::add_at_max_one_request_per_processor_spin()
{
	// For each processor k, different from the one under observation
	foreach_cluster_except(info, cluster, k)
	{
		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			LinearExpression *LHS = new LinearExpression();
			unsigned long RHS = 0;

			// For each task T_x in processor k
			foreach_task_in_cluster(info.get_tasks(), k, T_x)
			{
				const unsigned int x = T_x->get_id();
				var_t X_SPIN    = vars.spin(x, q);
				LHS->add_var(X_SPIN);
			}

			// For each task T_i in the processor under observation
			foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
			{
				unsigned long njobs = 0;

				if (lp_type == PDC_MODE)
					njobs = T_i->get_pedf_PDC_max_num_local_jobs(interval_length);
				if (lp_type == AC_MODE)
					njobs = T_i->get_pedf_AC_max_num_local_jobs(interval_length);

				RHS += njobs * T_i->get_num_requests(q);
			}

			add_inequality(LHS,RHS);
		}
	}
}

// Constraint 16: Per-task bound on spin delay
void MSRP_LP::add_per_task_bound_spin_delay()
{
	foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
	{
		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			unsigned long RHS = 0;
			const unsigned int x = T_x->get_id();
			var_t X_SPIN = vars.spin(x, q);

			foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
			{
				const unsigned long nrjobs_T_i = T_i->get_pedf_max_num_remote_jobs(T_x->get_deadline());
				const unsigned long nrjobs_T_x = T_x->get_pedf_max_num_remote_jobs(interval_length);

				RHS += nrjobs_T_i * T_i->get_num_requests(q) * nrjobs_T_x;
			}

			LinearExpression *exp = new LinearExpression();
			exp->add_var(X_SPIN);
			add_inequality(exp,RHS);
		}
	}
}

// Constraint 17: At max one remote request per processor for arrival blocking
void MSRP_LP::add_at_max_one_request_per_processor_arrival()
{
	// For each processor k, different from the one under observation
	foreach_cluster_except(info, cluster, k)
	{
		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			LinearExpression *LHS = new LinearExpression();
			var_t A_q = vars.indicator_arrival(q);

			// For each task T_x in processor k
			foreach_task_in_cluster(info.get_tasks(), k, T_x)
			{
				const unsigned int x = T_x->get_id();

				var_t X_ARRIVAL    = vars.arrival(x, q);
				LHS->add_var(X_ARRIVAL);
			}
			// LHS <= A_q
			LHS->sub_var(A_q);
			add_inequality(LHS,0);
		}
	}
}

// ------------------------------------------------------------------
//--------------[ B L O C K I N G     M E T H O D S]-----------------
// ------------------------------------------------------------------

class PEDFBlockingAnalysisMSRP : public PEDFBlockingAnalysis
{

private:
	unsigned long compute_blocking_PDC(unsigned long interval_length);
	unsigned long compute_blocking_AC (unsigned long interval_length);

public:
	PEDFBlockingAnalysisMSRP(const ResourceSharingInfo& info,
	                         unsigned int cluster)
		: PEDFBlockingAnalysis(info, cluster) {}

};

unsigned long PEDFBlockingAnalysisMSRP::compute_blocking_PDC(unsigned long interval_length)
{
	MSRP_LP mip(info, PDC_MODE, interval_length, cluster);

	return mip.solve(false);
}
unsigned long PEDFBlockingAnalysisMSRP::compute_blocking_AC (unsigned long interval_length)
{
	MSRP_LP mip(info, AC_MODE, interval_length, cluster);

	return mip.solve(false);
}

// ------------------------------------------------------------------
// --------------------[ E N T R Y    P O I N T ]--------------------
// ------------------------------------------------------------------

bool lp_pedf_msrp_is_schedulable(const ResourceSharingInfo& info)
{
	foreach_cluster(info, k)
	{
#ifdef __DEBUG_PEDF_BLK_ANALYSIS__
		std::cout << "[MSRP] CPU#" << k << std::endl;
#endif

		// Perform schedulability analysis for each processor k
		PEDFBlockingAnalysisMSRP analysis(info, k);
		if (!analysis.is_schedulable())
			return false;
	}

	return true;
}