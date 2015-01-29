#include <sstream>
#include <iostream>

#include "lp_common.h"

std::string VarMapperBase::var2str(unsigned int var) const
{
	uint64_t key;

	if (search_key_for_var(var, key))
	{
		return key2str(key, var);
	}
	else
		return "<?>";
}

std::string VarMapperBase::key2str(uint64_t key, unsigned int var) const
{
	std::ostringstream buf;
	buf << "X" << var;
	return buf.str();
}

hashmap<unsigned int, std::string> VarMapperBase::get_translation_table() const
{
	hashmap<unsigned int, std::string> table;

	foreach(map, kv)
	{
		table[kv->second] = key2str(kv->first, kv->second);
	}

	return table;
}

std::string VarMapper::key2str(uint64_t key, unsigned int var) const
{
	std::ostringstream buf;

	switch (get_type(key))
	{
		case BLOCKING_DIRECT:
			buf << "Xd[";
			break;
		case BLOCKING_INDIRECT:
			buf << "Xi[";
			break;
		case BLOCKING_PREEMPT:
			buf << "Xp[";
			break;
		case BLOCKING_OTHER:
			buf << "Xo[";
			break;
		default:
			buf << "X?[";
	}

	buf << get_task(key) << ", "
		<< get_res_id(key) << ", "
		<< get_req_id(key) << "]";

	return buf.str();
}

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

// This version is for partitioned shared-memory protocols where each
// task executes its critical section on its assigned processor.
void set_blocking_objective_part_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
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
		bool local = tx->get_cluster() == ti.get_cluster();

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			double length = request->get_request_length();;

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
}

// This version is for suspension-oblivious shared-memory protocols,
// where the analysis does not differentiate among the different kinds
// of blocking (since they are all just added to the execution time anyway).
void set_blocking_objective_sob(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
        LinearExpression *obj;

	obj = lp.get_objective();

	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			double length = request->get_request_length();;

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;

				var_id = vars.lookup(t, q, v, BLOCKING_SOB);
				obj->add_term(length, var_id);
			}
		}
	}
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


// Constraint 10 in [Brandenburg 2013]
// For shared-memory protocols.
// Remote tasks cannot preempt Ti since they are not scheduled
// on Ti's assigned task; therefore force BLOCKING_PREEMPT to zero.
void add_topology_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	LinearExpression *exp = new LinearExpression();

	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
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

// Constraint 9 in [Brandenburg 2013]
// local higher-priority tasks never cause blocking under SHM protocols
// assuming partitioned scheduling
void add_local_higher_priority_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	LinearExpression *exp = new LinearExpression();

	foreach_local_task(info.get_tasks(), ti, tx)
	{
		if (tx->get_priority() < ti.get_priority())
		{
			unsigned int t = tx->get_id();
			foreach(tx->get_requests(), request)
			{
				unsigned int q = request->get_resource_id();
				foreach_request_instance(*request, ti, v)
				{
					unsigned int var_id;
					var_id = vars.lookup(t, q, v,
					                     BLOCKING_PREEMPT);
					exp->add_var(var_id);

					var_id = vars.lookup(t, q, v,
					                     BLOCKING_INDIRECT);
					exp->add_var(var_id);

					var_id = vars.lookup(t, q, v,
					                     BLOCKING_DIRECT);
					exp->add_var(var_id);
				}
			}
		}
	}
	lp.add_equality(exp, 0);
}

static unsigned int max_num_arrivals_shm(
	const ResourceSharingInfo& info,
	const TaskInfo& ti)
{
	hashmap<unsigned int, unsigned int> request_counts;

	foreach(ti.get_requests(), req)
		request_counts[req->get_resource_id()] = 0;

	// count how often each resource is accessed on remote cores
	foreach_remote_task(info.get_tasks(), ti, tx)
	{
		foreach(tx->get_requests(), req)
		{
			unsigned int q = req->get_resource_id();
			if (request_counts.find(q) != request_counts.end())
				request_counts[q] += req->get_max_num_requests(ti.get_response());
		}
	}

	// initialize to 1 to account for job release
	unsigned int total = 1;

	foreach(ti.get_requests(), req)
		total += std::min(request_counts[req->get_resource_id()],
		                  req->get_num_requests());

	return total;
}

// Constraint 11 in [Brandenburg 2013]
// Local lower-priority tasks block at most once each time
// that Ti suspends (and once after release).
void add_local_lower_priority_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp)
{
	unsigned int num_arrivals = max_num_arrivals_shm(info, ti);

	foreach_local_lowereq_priority_task_except(info.get_tasks(), ti, tx)
	{
		LinearExpression *exp = new LinearExpression();
		unsigned int t = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();
			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				var_id = vars.lookup(t, q, v,
						     BLOCKING_PREEMPT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v,
						     BLOCKING_INDIRECT);
				exp->add_var(var_id);

				var_id = vars.lookup(t, q, v,
						     BLOCKING_DIRECT);
				exp->add_var(var_id);
			}
		}
		lp.add_equality(exp, num_arrivals);
	}
}
