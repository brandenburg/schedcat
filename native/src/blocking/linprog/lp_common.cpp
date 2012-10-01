#include "lp_common.h"

// LP-based analysis of semaphore protocols.
// Based on the paper:
// B. Brandenburg, "Improved Analysis and Evaluation of Real-Time Semaphore
// Protocols for P-FP Scheduling", Proceedings of the 19th IEEE Real-Time and
// Embedded Technology and Applications Symposium (RTAS 2013), April 2013.

void set_blocking_objective(
	VarMapper& vars,
	const ResourceSharingInfo& info, const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp,
	LinearExpression *local_obj,
	LinearExpression *remote_obj)
{
        LinearExpression *obj;

	obj = lp.get_objective();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			bool local = locality[q] == (int) ti.get_cluster();
			double length;

			// Sanity check topology info in debug mode.
			assert(locality[q] != NO_CPU);

			length = request->get_request_length();

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;

				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				obj->add_term(length, var_id);
				if (local && local_obj)
					local_obj->add_term(length, var_id);
				else if (!local && remote_obj)
					remote_obj->add_term(length, var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				obj->add_term(length, var_id);
				if (local && local_obj)
					local_obj->add_term(length, var_id);
				else if (!local && remote_obj)
					remote_obj->add_term(length, var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_PREEMPT);
				obj->add_term(length, var_id);
				if (local && local_obj)
					local_obj->add_term(length, var_id);
				else if (!local && remote_obj)
					remote_obj->add_term(length, var_id);
			}
		}
	}
#ifndef CONFIG_MERGED_LINPROGS
	// We have enumerated all relevant variables. Do not allow any more to
	// be created.
	vars.seal();
#endif
}

// Constraint 1 in [Brandenburg 2013]
void add_mutex_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
			{
				LinearExpression *exp = new LinearExpression();
				unsigned int var_id;

				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v, BLOCKING_PREEMPT);
				exp->add_var(var_id);

				lp.add_inequality(exp, 1);
			}
		}
	}
}

// Constraint 2 in [Brandenburg 2013]
void add_topology_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	LinearExpression *exp = new LinearExpression();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach_remote_request(tx->get_requests(), locality, ti, request)
		{
			unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v, BLOCKING_PREEMPT);
				exp->add_var(var_id);
			}
		}
	}
	lp.add_equality(exp, 0);
}

static unsigned int max_num_arrivals_remote(
	const ResourceLocality& locality,
	const TaskInfo& ti)
{
	// initialize to 1 to account for job release
	unsigned int count = 1;

	// count how often resources on remote cores are accessed
	foreach(ti.get_requests(), req)
		if (locality[req->get_resource_id()] != (int) ti.get_cluster())
			count += req->get_num_requests();

	return count;
}


// Constraint 3 in [Brandenburg 2013]
// One priority-boosting-related preemption per local task
// each time that Ji arrives (is released or resumed)
void add_local_lower_priority_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	unsigned int num_arrivals = max_num_arrivals_remote(locality, ti);

	foreach_local_lowereq_priority_task_except(info.get_tasks(), ti, tx)
	{
		LinearExpression *exp = new LinearExpression();

		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			// is it a resource local to Ti?
			if (locality[q] == (int) ti.get_cluster())
			{
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v,
					                     BLOCKING_PREEMPT);
					exp->add_var(var_id);
				}
			}
		}
		lp.add_equality(exp, num_arrivals);
	}
}

