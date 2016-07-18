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

#include "lp_pedf_spinlocks_common.h"

std::string SpinVarMapper::key2str(uint64_t key, unsigned int var) const
{
	lookup_key_t k;
	std::ostringstream buf;

	k.raw = key;

	switch (k.var.variable_type)
	{
	case SPIN_BLOCKING:
		buf << "Xs";
		break;
	case ARRIVAL_BLOCKING:
		buf << "Xa";
		break;
	case INDICATOR_ARRIVAL_BLOCKING:
		buf << "A";
		break;
	case CANCELLATIONS:
		buf << "C";
		break;
	}

	buf << "["
	    << k.var.tid << ", "
	    << k.var.rid << "]";

	return buf.str();
}


// ------------------------------------------------------------------
// -----------------------------[ L P ]------------------------------
// ------------------------------------------------------------------


PEDFBlockingAnalysisLP_Spinlocks::PEDFBlockingAnalysisLP_Spinlocks(
    const ResourceSharingInfo& _info,
    analysis_type_t atype,
    unsigned long delta,
    unsigned int _cluster)
	: taskset(_info.get_tasks()),
	  info(_info),
	  lp_type(atype),
	  interval_length(delta),
	  cluster(_cluster),
	  all_resources(get_all_resources(_info))
{
	// Add generic constraints
	add_no_arrival_blocking_dline_inside_interval();
	add_no_spin_delay_local_requests();
	add_joint_upper_bound_remote_requests();

	if (atype == AC_MODE)
		add_no_arrival_blocking();
	if (atype == PDC_MODE)
		add_arrival_blocking_single_resource();

	add_no_requests_no_arrival_blocking();
	add_arrival_blocking_max_one_local_request();
	add_exclude_non_conflicting_local_resources();

	set_objective();
}
unsigned long PEDFBlockingAnalysisLP_Spinlocks::solve(bool verbose)
{
	Solution *sol;
	double result;

	add_constraints_post_ctor();

	if (verbose)
	{
		hashmap<unsigned int, std::string> var_map;

		var_map = vars.get_translation_table();

		std::cout << std::endl
		          << "=====================================================" << std::endl;
		std::cout << "LP for t=" << interval_length << ":" << std::endl;
		pretty_print_linear_program(std::cout, *this, var_map) << std::endl;

		sol = linprog_solve(*this, vars.get_num_vars());
		result = floor(sol->evaluate(*get_objective()));

		std::cout << "Solution: " << result << std::endl;
		for (unsigned int x = 0; x < vars.get_num_vars(); x++)
		{
			std::cout << "X" << x
			          << ": " << var_map[x]
			          << " = " << sol->get_value(x)
			          << std::endl;
		}
	}
	else
	{
		sol = linprog_solve(*this, vars.get_num_vars());

		result = floor(sol->evaluate(*get_objective()));
	}

	delete sol;
	assert(result < ULONG_MAX);
	return (unsigned long) result;
}

// ------------------------------------------------------------------
// ----------------------[ O B J E C T I V E ]-----------------------
// ------------------------------------------------------------------
void PEDFBlockingAnalysisLP_Spinlocks::set_objective()
{
	LinearExpression *obj = get_objective();

	foreach(taskset, T_x)
	{
		const unsigned int x = T_x->get_id();

		foreach(all_resources, q_iter)
		{
			const unsigned int q = *q_iter;
			const double length = T_x->get_request_length(q);

			var_t X_SPIN    = vars.spin(x, q);
			var_t X_ARRIVAL = vars.arrival(x, q);

			// X_SPIN>=0, X_ARRIVAL>=0
			declare_variable_bounds(X_SPIN,     true, 0, false, 0);
			declare_variable_bounds(X_ARRIVAL,  true, 0, false, 0);

			if (length > 0)
			{
				obj->add_term(length, X_SPIN);
				obj->add_term(length, X_ARRIVAL);
			}
		}
	}
}

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------

// Constraint 8: No arrival blocking from tasks with d_i <= t
void PEDFBlockingAnalysisLP_Spinlocks::add_no_arrival_blocking_dline_inside_interval()
{
	foreach_task_in_cluster_having_leq_dline(taskset, cluster, interval_length, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(T_i->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			var_t X_ARRIVAL = vars.arrival(i, q);
			LinearExpression *exp = new LinearExpression();
			exp->add_var(X_ARRIVAL);
			// X_ARRIVAL <= 0
			add_inequality(exp, 0);
		}
	}
}

// Constraint 9: No spin delay from local requests
void PEDFBlockingAnalysisLP_Spinlocks::add_no_spin_delay_local_requests()
{
	LinearExpression *exp = new LinearExpression();

	foreach_task_in_cluster(taskset, cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(T_i->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			var_t X_SPIN = vars.spin(i, q);
			exp->add_var(X_SPIN);
		}
	}

	add_inequality(exp, 0);
}

// Constraint 10: Joint upper-bound for remote requests
void PEDFBlockingAnalysisLP_Spinlocks::add_joint_upper_bound_remote_requests()
{
	foreach_task_not_in_cluster(taskset, cluster, T_i)
	{
		const unsigned int i = T_i->get_id();
		unsigned int nrjobs = T_i->get_pedf_max_num_remote_jobs(interval_length);

		foreach(T_i->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			unsigned int N_i_q = request->get_num_requests();

			var_t X_ARRIVAL = vars.arrival(i, q);
			var_t X_SPIN    = vars.spin(i, q);

			LinearExpression *exp = new LinearExpression();
			exp->add_var(X_ARRIVAL);
			exp->add_var(X_SPIN);

			add_inequality(exp, nrjobs * N_i_q);
		}
	}
}

// Constraint 11: Only one resource can cause arrival blocking
void PEDFBlockingAnalysisLP_Spinlocks::add_arrival_blocking_single_resource()
{
	LinearExpression *exp = new LinearExpression();
	foreach(all_resources,q_iter)
	{
		const unsigned int q = *q_iter;

		var_t A_q = vars.indicator_arrival(q);
		declare_variable_binary(A_q);

		exp->add_var(A_q);
	}

	add_inequality(exp, 1);
}

// Constraint 11-bis: No arrival blocking at all (for AC)
void PEDFBlockingAnalysisLP_Spinlocks::add_no_arrival_blocking()
{
	LinearExpression *exp = new LinearExpression();
	foreach(all_resources,q_iter)
	{
		const unsigned int q = *q_iter;

		var_t A_q = vars.indicator_arrival(q);
		declare_variable_binary(A_q);

		exp->add_var(A_q);
	}

	add_inequality(exp, 0);
}

// Constraint 12: Exclude non-conflicting local resources
void PEDFBlockingAnalysisLP_Spinlocks::add_exclude_non_conflicting_local_resources()
{
	std::set<unsigned int> non_conflicting_resources;
	ResourceSet local_resources = get_local_resources(info);
	PriorityCeilings ceilings   = get_priority_ceilings(info);

	// Compute the maximum preemption level for tasks having dline <= t
	unsigned int max_preemption_level = 0;
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		if (T_i->get_deadline() <= interval_length)
			if (T_i->get_priority() > max_preemption_level)
				max_preemption_level = T_i->get_priority();
	}

	// Identify non-conflicting local resources
	foreach(local_resources, resource)
	{
		if (ceilings[*resource] >= max_preemption_level)
			non_conflicting_resources.insert(*resource);
	}

	LinearExpression *exp = new LinearExpression();
	foreach(non_conflicting_resources, q_iter)
	{
		unsigned int var_id = vars.indicator_arrival(*q_iter);
		exp->add_var(var_id);
	}
	if (exp->has_terms())
		add_inequality(exp, 0);
	else
		delete exp;
}


// Constraint 13: Exclude arrival blocking when there are no requests
void PEDFBlockingAnalysisLP_Spinlocks::add_no_requests_no_arrival_blocking()
{
	foreach(all_resources,q_iter)
	{
		const unsigned int q = *q_iter;
		unsigned int n_reqs = 0;

		foreach_task_in_cluster_having_gt_dline(info.get_tasks(), cluster, interval_length, T_i)
		n_reqs += T_i->get_num_requests(q);

		var_t A_q = vars.indicator_arrival(q);
		declare_variable_binary(A_q);

		LinearExpression *exp = new LinearExpression();
		exp->add_var(A_q);
		// A_q <= n_reqs
		add_inequality(exp, n_reqs);
	}
}

// Constraint 14: At maximum one local request can cause arrival blocking
void PEDFBlockingAnalysisLP_Spinlocks::add_arrival_blocking_max_one_local_request()
{
	foreach(all_resources,q_iter)
	{
		const unsigned int q = *q_iter;

		LinearExpression *exp = new LinearExpression();
		var_t A_q = vars.indicator_arrival(q);

		foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
		{
			const unsigned int i = T_i->get_id();

			var_t X_ARRIVAL = vars.arrival(i, q);
			exp->add_var(X_ARRIVAL);
		}

		// expr <= A_q
		exp->sub_var(A_q);
		add_inequality(exp, 0);
	}
}