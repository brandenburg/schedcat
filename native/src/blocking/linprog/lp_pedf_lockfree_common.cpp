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

#include "lp_pedf_lockfree_common.h"

std::string LockFreeVarMapper::key2str(uint64_t key, unsigned int var) const
{
	lookup_key_t k;
	std::ostringstream buf;

	k.raw = key;

	switch (k.var.variable_type)
	{
	case LOCAL_CONFLICT:
		buf << "Y_L";
		break;
	case REMOTE_CONFLICT:
		buf << "Y_R";
		break;
	case INDICATOR_ARRIVAL_BLOCKING:
		buf << "A";
		break;
	}

	buf << "["
	    << k.var.tid_i << ", "
	    << k.var.tid_j << ", "
	    << k.var.rid << "]";

	return buf.str();
}


// ------------------------------------------------------------------
// -----------------------------[ L P ]------------------------------
// ------------------------------------------------------------------


PEDFBlockingAnalysisLP_LockFree::PEDFBlockingAnalysisLP_LockFree(
    const ResourceSharingInfo& _info,
    analysis_type_t atype,
    unsigned long delta,
    unsigned int _cluster,
    unsigned long blocking_LB,
    unsigned long blocking_UB,
    bool relax)
	: taskset(_info.get_tasks()),
	  info(_info),
	  lp_type(atype),
	  interval_length(delta),
	  cluster(_cluster),
	  all_resources(get_all_resources(_info))
{
	integer_relaxation = relax;

	// Add generic constraints

	add_blocking_upper_and_lower_bound(blocking_LB,blocking_UB);
	add_no_retries_for_resources_not_accessed();
	add_one_retry_for_at_most_one_remote_commit();

	if (lp_type == AC_MODE)
		add_no_arrival_blocking();

	set_objective();
}
unsigned long PEDFBlockingAnalysisLP_LockFree::solve(bool verbose)
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
void PEDFBlockingAnalysisLP_LockFree::set_objective()
{
	LinearExpression *obj = get_objective();

	foreach_task_in_cluster(taskset, cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources, q_iter)
		{
			const unsigned int q = *q_iter;
			const double length = T_i->get_request_length(q);

			var_t Y_R_i_q  = vars.remote_conflicts (i, q);
			var_t A_i_q    = vars.indicator_arrival(i, q);

			// Variables >= 0
			declare_variable_bounds(Y_R_i_q, true, 0, false, 0);
			declare_variable_bounds(A_i_q  , true, 0, false, 0);

			if (!integer_relaxation)
			{
				declare_variable_integer(Y_R_i_q);
				declare_variable_integer(A_i_q);
			}

			obj->add_term(length, Y_R_i_q);
			obj->add_term(length, A_i_q);

			foreach_task_in_cluster(taskset, cluster, T_j)
			{
				const unsigned int j = T_j->get_id();
				var_t Y_L_i_j_q  = vars.local_conflicts(i, j, q);

				// Y_R_i_j_q >= 0
				declare_variable_bounds(Y_L_i_j_q, true, 0, false, 0);

				if (!integer_relaxation)
					declare_variable_integer(Y_L_i_j_q);

				obj->add_term(length, Y_L_i_j_q);
			}
		}
	}
}

// ------------------------------------------------------------------
// --------------------[ C O N S T R A I N T S ]---------------------
// ------------------------------------------------------------------



void PEDFBlockingAnalysisLP_LockFree::add_blocking_upper_and_lower_bound(
    unsigned long blocking_LB,
    unsigned long blocking_UB)
{
	LinearExpression *obj_minus = new LinearExpression();
	LinearExpression *obj_plus  = new LinearExpression();

	foreach_task_in_cluster(taskset, cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources, q_iter)
		{
			const unsigned int q = *q_iter;
			const double length = T_i->get_request_length(q);

			var_t Y_R_i_q  = vars.remote_conflicts(i, q);
			var_t A_i_q    = vars.indicator_arrival(i, q);

			obj_plus->add_term(length, Y_R_i_q);
			obj_plus->add_term(length, A_i_q);

			obj_minus->sub_term(length, Y_R_i_q);
			obj_minus->sub_term(length, A_i_q);

			foreach_task_in_cluster(taskset, cluster, T_j)
			{
				const unsigned int j = T_j->get_id();
				var_t Y_R_i_j_q  = vars.local_conflicts(i, j, q);

				obj_plus->add_term(length, Y_R_i_j_q);
				obj_minus->sub_term(length, Y_R_i_j_q);
			}
		}
	}

	const double lb = (double)blocking_LB - 1.0 > 0 ? 0.0 : (double)blocking_LB - 1.0;

	// obj >= LB  ==>  LB <= obj  ==>  LB - obj <= 0  ==> -obj <= -LB
	add_inequality(obj_minus, -lb);


	if (blocking_UB > 0)
		add_inequality(obj_plus, blocking_UB);
}

void PEDFBlockingAnalysisLP_LockFree::add_no_retries_for_resources_not_accessed()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			// Skip if N_{i,q}>0
			if (T_i->get_num_requests(q) > 0)
				continue;

			var_t Y_R_i_q = vars.remote_conflicts(i,q);

			exp->add_var(Y_R_i_q);

			foreach_task_in_cluster(info.get_tasks(), cluster, T_j)
			{
				const unsigned int j = T_j->get_id();
				var_t Y_L_i_j_q = vars.local_conflicts(i, j, q);

				exp->add_var(Y_L_i_j_q);
			}
		}
	}

	add_inequality(exp, 0);
}

void PEDFBlockingAnalysisLP_LockFree::add_one_retry_for_at_most_one_remote_commit()
{
	foreach(all_resources,q_iter)
	{
		const unsigned int q = *q_iter;

		LinearExpression *exp = new LinearExpression();

		foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
		{
			const unsigned int i = T_i->get_id();
			var_t Y_R_i_q = vars.remote_conflicts(i, q);
			exp->add_var(Y_R_i_q);
		}

		unsigned long RHS = 0;
		foreach_task_not_in_cluster(info.get_tasks(), cluster, T_x)
		RHS += T_x->get_pedf_max_num_remote_jobs(interval_length) * T_x->get_num_requests(q);

		add_inequality(exp, RHS);
	}
}


// No arrival blocking at all (for AC or preemptive commit loops)
void PEDFBlockingAnalysisLP_LockFree::add_no_arrival_blocking()
{
	LinearExpression *exp = new LinearExpression();
	foreach_task_in_cluster(info.get_tasks(), cluster, T_i)
	{
		const unsigned int i = T_i->get_id();

		foreach(all_resources,q_iter)
		{
			const unsigned int q = *q_iter;

			var_t A_i_q = vars.indicator_arrival(i, q);

			exp->add_var(A_i_q);
		}
	}
	add_inequality(exp, 0);
}