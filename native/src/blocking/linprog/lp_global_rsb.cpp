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

#include "lp_global.h"

GlobalRestrictedSegmentBoostingLP::GlobalRestrictedSegmentBoostingLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: GlobalSuspensionAwareLP(info, task_index, number_of_cpus)
{
		// Constraint 16
		add_rsb_co_boosting_stalling_interference_to_csl();
		// Constraint 17
		add_rsb_total_co_boosting_stalling_interference();
		// Constraint 18
		add_rsb_co_boosting_interference();
		// Constraint 19
		add_rsb_total_co_boosting_interference();
		// Constraint 20
		add_rsb_no_stalling_interference();
		// Constraint 21
		add_rsb_m_highest_constraint();
		// Constraint 22
		add_rsb_indirect_constraint();
}

// The maximum resource-holding H_x,q under RSB
// according to Lemma 9 in the paper
unsigned long GlobalRestrictedSegmentBoostingLP::resource_hold_time(
		unsigned int tx_id, unsigned int res_id)
{
	unsigned long rht = 0, max_csl = 0;
	const TaskInfo &tx = taskset[tx_id];
	assert(tx_id == tx.get_id());

	rht = tx.get_request_length(res_id);

	if (!rht)
		return 0;

	foreach_task_except(taskset, ti, ta)
	{
		if (ta->get_id() != tx_id)
		{
			max_csl = 0;
			foreach(ta->get_requests(), req)
			{
				if (req->get_resource_id() != res_id)
					if (req->get_request_length() > max_csl)
						max_csl = req->get_request_length();
			}
			rht += max_csl;
		}
	}

	return rht;
}


//Constraints for restricted segment boosting

// Constraint 16: co-boosting and stalling interference due to a task Tx
// is limited to the total cumulative critical section of (i) all higher-base-priority tasks
// subtract the direct blocking that Ji incurred due to these tasks, and (ii)
// indirect and preemption pi-blocking due to all lower-base-priority tasks
// except Tx
void GlobalRestrictedSegmentBoostingLP::add_rsb_co_boosting_stalling_interference_to_csl()
{
	unsigned long hp_csl = 0;

	//total cumulative critical section length of all higher-base-priority tasks
	foreach_higher_priority_task(taskset, ti, th)
	{
		unsigned int njobs = th->get_max_num_jobs(ti.get_response());

		foreach(th->get_requests(), req)
		{
			unsigned int res_id = req->get_resource_id();
			unsigned int length = req->get_request_length();

			hp_csl += njobs * th->get_num_requests(res_id) * length;
		}
	}

	foreach_lower_priority_task(taskset, ti, tx)
	{
		LinearExpression *exp = new LinearExpression();

		const unsigned int tx_id = tx->get_id();

		exp->add_var(vars.co_boosting_interference(tx_id));
		exp->add_var(vars.stalling_interference(tx_id));

		foreach_task_except(taskset, ti, ta)
		{
			unsigned int ta_id = ta->get_id();

			foreach(ta->get_requests(), req)
			{
				const unsigned int q = req->get_resource_id();
				const unsigned int csl = req->get_request_length();

				if (ta_id < ti.get_id())
				{
					foreach_request_instance(*req, ti, v)
						exp->add_term(csl, vars.direct(ta_id, q, v));
				}

				if ((ta_id > ti.get_id()) && (ta_id != tx_id))
				{
					foreach_request_instance(*req, ti, v)
					{
						exp->sub_term(csl, vars.indirect(ta_id, q, v));
						exp->sub_term(csl, vars.preemption(ta_id, q, v));
					}
				}
			}
		}

		add_inequality(exp, hp_csl);
	}
}

// Constraint 17: in addition to Constraint 17, at each point in time,
// at most m-1 tasks cause J_i to incur co-boosting and stalling interference
void GlobalRestrictedSegmentBoostingLP::add_rsb_total_co_boosting_stalling_interference()
{
	unsigned long hp_csl = 0;
	const double m_inv = 1.0/(m - 1);

	//total cumulative critical section length of all higher-base-priority tasks
	foreach_higher_priority_task(taskset, ti, th)
	{
		unsigned int njobs = th->get_max_num_jobs(ti.get_response());

		foreach(th->get_requests(), req)
		{
			unsigned int res_id = req->get_resource_id();
			unsigned int length = req->get_request_length();

			hp_csl += njobs * th->get_num_requests(res_id) * length;
		}
	}

	LinearExpression *exp = new LinearExpression();
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		if (tx_id > ti.get_id())
		{
			// LHS of the constraint in the paper, divided by (m-1)
			exp->add_term(m_inv, vars.co_boosting_interference(tx_id));
			exp->add_term(m_inv, vars.stalling_interference(tx_id));
		}

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			const unsigned int csl = request->get_request_length();
			foreach_request_instance(*request, ti, v)
			{
				if (tx_id < ti.get_id())
					exp->add_term(csl, vars.direct(tx_id, q, v));
				// subtract indirect and preemption blocking on the left-hand-side
				if (tx_id > ti.get_id())
				{
					exp->sub_term(csl, vars.indirect(tx_id, q, v));
					exp->sub_term(csl, vars.preemption(tx_id, q, v));
				}
			}
		}
	}
	add_inequality(exp, hp_csl);
}

// Constraint 18: Tx causes Ji to incur co-boosting interference only if
// another task Ta causes Ji to incur indirect or preemption blocking
void GlobalRestrictedSegmentBoostingLP::add_rsb_co_boosting_interference()
{
	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		LinearExpression *exp = new LinearExpression();

		exp->add_var(vars.co_boosting_interference(tx_id));

		// for tasks with base-priority lower than tx
		foreach_lower_priority_task(taskset, *tx, ta)
		{
			unsigned int ta_id = ta->get_id();

			foreach(ta->get_requests(), request)
			{
				const unsigned int q = request->get_resource_id();
				const unsigned int csl = request->get_request_length();
				foreach_request_instance(*request, ti, v)
				{
					exp->sub_term(csl, vars.indirect(ta_id, q, v));
					exp->sub_term(csl, vars.preemption(ta_id, q, v));
				}
			}
		}

		add_inequality(exp, 0);
	}

}

// Constraint 19: in addition to Constraint 19, at most m - 1 tasks
// cause Ji to incur co-boosting interference at any point in time
void GlobalRestrictedSegmentBoostingLP::add_rsb_total_co_boosting_interference()
{
	const double m_inv = 1.0/(m - 1);
	LinearExpression *exp = new LinearExpression();
	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		exp->add_term(m_inv, vars.co_boosting_interference(tx_id));

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			const unsigned int csl = request->get_request_length();
			foreach_request_instance(*request, ti, v)
			{
				exp->sub_term(csl, vars.indirect(tx_id, q, v));
				exp->sub_term(csl, vars.preemption(tx_id, q, v));
			}
		}
	}
	add_inequality(exp, 0);

}

// Constraint 20: rule out stalling interference of each lower-base-priority
// task Tx if all of Tx's lower-base-priority tasks do not access any resource
// requested by Ti
void GlobalRestrictedSegmentBoostingLP::add_rsb_no_stalling_interference()
{
	// find highest-priority task Th with priority less than Ti that
	// accesses a resources used by Ti, but no lower-priority task does.
	unsigned int h;

	for (h = taskset.size() - 1; h > i; h--)
	{
		// check task of priority h - 1
		const TaskInfo& th = taskset[h];

		bool overlap = false;
		foreach(th.get_requests(), req)
		{
			unsigned int q = req->get_resource_id();
			if (ti.get_num_requests(q) > 0)
			{
				overlap = true;
				break;
			}
		}
		if (overlap)
			break;
	}

	LinearExpression *exp = new LinearExpression();

	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		// each task with id > index does not access any resource
		// that will be requested by T_i
		if (tx_id >= h)
			exp->add_var(vars.stalling_interference(tx->get_id()));
	}

	add_inequality(exp, 0);
}

// Constraint 21: the m highest base priority task will not cause Ti (T_i is also one of
// the m highest base priority task) to incur any preemption blocking
void GlobalRestrictedSegmentBoostingLP::add_rsb_m_highest_constraint()
{
	if (ti.get_id() < m - 1)
	// If the tighter condition is not met, no constraint will be added anyway.
	{
		LinearExpression *exp = new LinearExpression();

		foreach_lower_priority_task(taskset, ti, tx)
		{
			const unsigned int x = tx->get_id();

			if (x < m)
			{
				foreach(tx->get_requests(), request)
				{
					const unsigned int q = request->get_resource_id();
					foreach_request_instance(*request, ti, v)
						exp->add_var(vars.preemption(x, q, v));
				}
			}
		}
		add_inequality(exp, 0);
	}
}

// Constraint 22: indirect pi-blocking due to Tx is bounded by the
// number of requests to the resources that will be requested by Ti
// and that are issued by all tasks except Ti and Tx
void GlobalRestrictedSegmentBoostingLP::add_rsb_indirect_constraint()
{
	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		unsigned int total = 0;

		// compute RHS
		foreach(ti.get_requests(), request)
		{
			if (request->get_num_requests() > 0)
			{
				unsigned int res_id = request->get_resource_id();

				foreach_task_except(taskset, ti, ty)
				{
					if (ty->get_id() == x)
						continue;

					unsigned int njobs =  ty->get_max_num_jobs(ti.get_response());
					total += njobs * ty->get_num_requests(res_id);
				}
			}
		}

		LinearExpression *exp = new LinearExpression();

		// LHS
		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
				exp->add_var(vars.indirect(x, q, v));
		}

		add_inequality(exp, total);
	}
}

