#include "lp_common.h"
#include "math-helper.h"
#include <set>
#include <map>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <climits>


bool add_prio_fifo_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool preemptive = false);

unsigned long get_max_CS_per_cluster(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	Clusters clusters;
	split_by_cluster(info, clusters);
	unsigned long sum = 0;

	for (unsigned int c=0; c < clusters.size(); c++)
	{
		if (ti.get_cluster() != c) // ignore Ti's cluster
		{
			unsigned int longest_CS_on_cluster = 0;
			foreach(clusters[c], task)
			{
				foreach((*task)->get_requests(), request)
				{
					if ((*request).get_resource_id() == res_id &&
						(*request).get_request_priority() == locking_prio)
					{
						longest_CS_on_cluster = std::max(longest_CS_on_cluster, (*request).get_request_length());
					}
				}
			}
		sum += longest_CS_on_cluster;
		}
	}

	return sum;
}


unsigned long get_SPlh(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned long W,
		std::set<unsigned int>& Qlh)
{
	unsigned long SPlh = 0;

	for (std::set<unsigned int>::iterator res = Qlh.begin(); res != Qlh.end(); res++)
	{
		foreach(info.get_tasks(), task)
		{
			if (task->get_cluster() == ti.get_cluster() &&
				task->get_priority() < ti.get_priority())
			{
				foreach(task->get_requests(), request)
				{
					if (request->get_resource_id() == res_id)
					{
						SPlh += divide_with_ceil(W, task->get_period())
								* request->get_num_requests()
								* get_max_CS_per_cluster(info, ti, res_id, request->get_request_priority());
					}

				}
			}
		}
	}
	return SPlh;
}


unsigned long get_max_lp_csl_from_cluster(
		const ResourceSharingInfo& info,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned int cluster)
{
	unsigned long max_csl = 0;

	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == cluster)
		{
			foreach(task->get_requests(), request)
			{
				if ((*request).get_resource_id() == res_id &&
					(*request).get_request_priority() > locking_prio)
				{
					max_csl = std::max(max_csl, (unsigned long) (*request).get_request_length());
				}
			}
		}
	}

	return max_csl;
}


unsigned long get_cpp_lsp(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long cpp_lsp = 0;
	Clusters clusters;
	split_by_cluster(info, clusters);

	for (unsigned int excluded_cluster = 0; excluded_cluster < clusters.size(); excluded_cluster++)
	{
		if (excluded_cluster != ti.get_cluster()) //skip Ti's cluster
		{

			// the one lower-prio request
			unsigned long cpp_lp = get_max_lp_csl_from_cluster(info, res_id, locking_prio, excluded_cluster);

			// get max. CSL for res with same-prio from each other cluster
			unsigned long cpp_sp = 0;
			for (unsigned int c=0; c < clusters.size(); c++)
			{
				if ((c != ti.get_cluster()) && (c != excluded_cluster)) // ignore Ti's cluster and excluded cluster
				{
					unsigned int longest_CS_on_cluster = 0;
					foreach(clusters[c], task)
					{
						foreach((*task)->get_requests(), request)
						{
							if ((*request).get_resource_id() == res_id &&
								(*request).get_request_priority() == locking_prio)
							{
								longest_CS_on_cluster = std::max(longest_CS_on_cluster, (*request).get_request_length());
							}
						}
					}
					cpp_sp += longest_CS_on_cluster;
				}
			}
			unsigned long cpp_lsp_current_cluster = cpp_lp + cpp_sp;

			cpp_lsp = std::max(cpp_lsp, cpp_lsp_current_cluster);
		}
	}

	return cpp_lsp;
}


unsigned long get_spin_L_prime(
		const ResourceSharingInfo& info,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned int Pa,
		unsigned int Pl)
{
	Clusters clusters;
	split_by_cluster(info, clusters);

	unsigned long cpp_lp = get_max_lp_csl_from_cluster(info, res_id, locking_prio, Pl);

	// get max. CSL for res with same-prio from each other cluster
	unsigned long cpp_sp = 0;
	for (unsigned int c=0; c < clusters.size(); c++)
	{
		if ((c != Pa) && (c != Pl)) // ignore Ti's cluster and excluded cluster
		{
			unsigned int longest_CS_on_cluster = 0;
			foreach(clusters[c], task)
			{
				foreach((*task)->get_requests(), request)
				{
					if ((*request).get_resource_id() == res_id &&
						(*request).get_request_priority() == locking_prio)
					{
						longest_CS_on_cluster = std::max(longest_CS_on_cluster, (*request).get_request_length());
					}
				}
			}
			cpp_sp += longest_CS_on_cluster;
		}
	}
	unsigned long cpp_lsp_current_cluster = cpp_lp + cpp_sp;

	return cpp_lsp_current_cluster;
}


unsigned long get_spin_L(
		const ResourceSharingInfo& info,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned int Pa)
{
	unsigned long max_spin_L = 0;

	Clusters clusters;
	split_by_cluster(info, clusters);

	for (unsigned int c=0; c < clusters.size(); c++)
	{
		if (c != Pa)
			max_spin_L = std::max(max_spin_L, get_spin_L_prime(info, res_id, locking_prio, Pa, c));
	}

	return max_spin_L;
}


unsigned long get_spin_LS(
		const ResourceSharingInfo& info,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned int Pa)
{
	unsigned long spin_S = 0, spin_L = 0;

	// compute spin_S
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() != Pa)
		{
			foreach(task->get_requests(), request)
			{
				if (request->get_resource_id() == res_id &&
					request->get_request_priority() == locking_prio)
				{
					spin_S = std::max(spin_S, (unsigned long) request->get_request_length());
				}
			}
		}
	}

	spin_L = get_spin_L(info, res_id, locking_prio, Pa);

	return std::max(spin_S, spin_L);
}


unsigned long get_cpp_PFP(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long cpp_i = 0, cpp_lh = 0;

	//compute cpp_i
	cpp_i = get_spin_LS(info, res_id, locking_prio, ti.get_cluster());

	// compute cpp^lh
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster() &&
			task->get_priority() < ti.get_priority()) //local higher-priority task
		{
			foreach(task->get_requests(), request)
			{
				if (request->get_num_requests() > 0)
					cpp_lh = std::max(cpp_lh,
									  get_spin_LS(info,
												  request->get_resource_id(),
												  request->get_request_priority(),
												  task->get_cluster()));
			}
		}
	}

	return std::max(cpp_i, cpp_lh);
}


unsigned int get_pi_r(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id)
{
	unsigned int min_prio = 0;
	bool first = true;

	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster()
			&& task->get_priority() < ti.get_priority())
		{
			foreach(task->get_requests(), request)
			{
				if (request->get_resource_id() == res_id)
				{
					if (first)
					{
						first = false;
						min_prio = request->get_request_priority();
					}
					else
						min_prio = std::max(min_prio, request->get_request_priority());
				}
			}
		}
	}
	return min_prio;
}


long bound_wait_time_prio_fifo_preemptive(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned long HP = 0;
	unsigned long wait_time = 0;

	std::set<unsigned int> Qlh = get_localHP_resources(info, ti);

	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() != ti.get_cluster())
		{
			foreach(task->get_requests(), request)
			{
				if (Qlh.find(request->get_resource_id()) != Qlh.end() ||
					request->get_resource_id() == res_id)
				{
					unsigned int min_pi_r = 0;
					if (request->get_resource_id() == res_id)
						min_pi_r = locking_prio;
					min_pi_r = std::max(min_pi_r, get_pi_r(info, ti, res_id));

					if (request->get_request_priority() < min_pi_r)
					{
						HP += request->get_request_length() * request->get_num_requests();
					}
				}
			}
		}
	}

	unsigned long LSP_lh = 0;
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() == ti.get_cluster()
			&& task->get_priority() < ti.get_priority())
		{
			foreach(task->get_requests(), request)
			{
				LSP_lh += request->get_num_requests() * get_spin_LS(info,
																	request->get_resource_id(),
																	request->get_request_priority(),
																	ti.get_cluster());
			}
		}
	}

	wait_time += HP;
	wait_time += LSP_lh;

	unsigned long LSP_i = get_spin_LS(info, res_id, locking_prio, ti.get_cluster());
	wait_time += LSP_i;

	unsigned long I = get_hp_interference(info, ti, wait_time);
	wait_time += I;

	unsigned long LSP_P = get_cpp_PFP(info, ti, res_id, locking_prio) * max_preemptions(info, ti, wait_time);
	wait_time += LSP_P;

	unsigned long estimate = 0, new_estimate = wait_time;

	// use RTA to find max. wait time
	while (estimate <= ti.get_period() && estimate != new_estimate)
	{
		estimate = new_estimate;

		HP = 0;
		foreach(info.get_tasks(), task)
		{
			if (task->get_cluster() != ti.get_cluster())
			{
				foreach(task->get_requests(), request)
				{
					if (Qlh.find(request->get_resource_id()) != Qlh.end() ||
						request->get_resource_id() == res_id)
					{
						unsigned int min_pi_r = 0;
						if (request->get_resource_id() == res_id)
							min_pi_r = locking_prio;
						min_pi_r = std::max(min_pi_r, get_pi_r(info, ti, res_id));

						if (request->get_request_priority() < min_pi_r)
						{
							HP += request->get_request_length() * request->get_max_num_requests(estimate);
						}
					}
				}
			}
		}

		LSP_lh = 0;
		foreach(info.get_tasks(), task)
		{
			if (task->get_cluster() == ti.get_cluster()
				&& task->get_priority() < ti.get_priority())
			{
				foreach(task->get_requests(), request)
				{
					LSP_lh += request->get_max_num_requests(estimate)
							  * get_spin_LS(info,
											request->get_resource_id(),
											request->get_request_priority(),
											ti.get_cluster());
				}
			}
		}

		I = get_hp_interference(info, ti, wait_time);
		LSP_P = get_cpp_PFP(info, ti, res_id, locking_prio) * max_preemptions(info, ti, wait_time);

		new_estimate = HP + LSP_i + LSP_lh + LSP_P + I + 1;
	}

	if (estimate <= ti.get_period())
		return estimate; // wait time converged; return wait time
	else
		return -1; // wait time didn't converge; return error value
}


long bound_wait_time_prio_fifo(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio)
{
	unsigned int pi_i_q = get_min_prio(ti, res_id); // minimum priority of any req. of Ti for res_id
	unsigned long wait_time = 0;
	unsigned long LP_Ti = 0;

	// choose initial value for wait_time bound: hit by each job with higher locking prio
	// and once by a single lower locking prio request
	foreach(info.get_tasks(), task)
	{
		if (task->get_cluster() != ti.get_cluster())
		{
			foreach(task->get_requests(), request)
			{
				if (request->get_resource_id() == res_id &&
					request->get_request_priority() > pi_i_q)
				{
					// this is a lower-prio request to the resource Ti accesses, lq
					// ==> keep track of the max. CSL
					LP_Ti = std::max(LP_Ti, (unsigned long) request->get_request_length());
				}
			}
		}
	}

	unsigned long delay_by_lower = 0;

	// Ti's request can be directly delayed once by a remote lower-prio request
	// ==> account for the longest remote CSL for the resource Ti accesses (LP_Ti)
	delay_by_lower += LP_Ti;

	wait_time += delay_by_lower;

	 // sum of longest CSLs for lq on each remote cluster
	unsigned long SP_Ti = get_max_CS_per_cluster(info, ti, res_id, pi_i_q);
	unsigned long delay_by_same = SP_Ti;

	wait_time += delay_by_same;

	unsigned long estimate = 0, new_estimate = wait_time;

	// use RTA to find max. wait time
	while (estimate <= ti.get_period() && estimate != new_estimate)
	{
		unsigned long delay_by_higher = 0;
		estimate = new_estimate;

		foreach(info.get_tasks(), task)
		{
			foreach(task->get_requests(), request)
			{
				if ((request->get_resource_id() == res_id) &&
					task->get_cluster() != ti.get_cluster())
				{
					if (request->get_request_priority() < locking_prio)
					{
						// this is a higher-prio request; add to total CSL
						// corresponds to the S(l_q,pi) term in the wait-time recurrence
						delay_by_higher += request->get_request_length()
										   * request->get_max_num_requests(estimate);
					}
				}
			}
		}

		// account for LP^Ti: blocking of Ti's request by remote lower-prio req.s
		delay_by_lower = LP_Ti;

		// account for SP^Ti: blocking of Ti's request by remote same-prio req.s
		delay_by_same = SP_Ti;

		new_estimate = delay_by_lower + delay_by_same + delay_by_higher;

		// add one epsilon to wait-time bound to ensure that Ti's request finally succeeds
		new_estimate += 1;
	}

	if (estimate <= ti.get_period())
		return estimate; // wait time converged; return wait time
	else
		return -1; // wait time didn't converge; return error value
}


// Constraint 16: Limit spin-delay due to higher-priority requests
// using the wait-time bound.
bool add_prio_fifo_direct_blocking_HP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks

		long wait_time_bound;
		if (preemptive)
			wait_time_bound = bound_wait_time_prio_fifo_preemptive(info, ti, *resource, min_prioHP);
		else
			wait_time_bound = bound_wait_time_prio_fifo(info, ti, *resource, min_prioHP);

		if (wait_time_bound < 0) // use response time if wait-time cannot be bounded
			wait_time_bound = ti.get_response();

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (ti.get_cluster() != c) // ignore Ti's cluster
			{
				foreach(clusters[c], task)
				{
					LinearExpression *exp = new LinearExpression();
					unsigned int max_num_reqs = 0;
					foreach((*task)->get_requests(), request)
					{
						if ((*request).get_resource_id() == *resource &&
							(*request).get_request_priority() < min_prioHP)
						{
							max_num_reqs += request->get_max_num_requests(wait_time_bound);
							foreach_request_instance(*request, ti, v)
							{
								unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_DIRECT);
								exp->add_var(var_id);
							}
						}
					}

					if (exp->has_terms())
						lp.add_inequality(exp, max_num_reqs * ncs);
					else
						delete exp;
				}
			}
		}
	}
	return true;
}


// Constraint 17: Limit direct blocking due to requests with the same locking priority
// to the number of requests issued by Ti and higher-prio tasks on the same processor
// while Ti is pending for each resource per processor.
// This sounds rather convoluted. Just check out Constraint 17.
void add_prio_fifo_max_direct_blocking_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive)
{
	Clusters clusters;
	split_by_cluster(info, clusters);
	std::set<unsigned int> all_resources = get_all_resources(info);

	foreach(all_resources, resource)
	{
		unsigned int min_prioHP = get_min_prio(info, ti, *resource, false);
		unsigned int niql = count_local_hp_reqs(info, ti, *resource); //count local hp requests for resource
		for (unsigned int c = 0; c < clusters.size(); c++)
		{
			if (c != ti.get_cluster())
			{
				LinearExpression *exp = new LinearExpression();

				foreach(clusters[c], task)
				{
					foreach((*task)->get_requests(), request)
					{
						unsigned int res_id = request->get_resource_id();
						if (res_id == *resource &&
							(*request).get_request_priority() == min_prioHP)
						{
							foreach_request_instance(*request, ti, v)
							{
								unsigned int var_id;
								var_id = vars.lookup((*task)->get_id(), *resource, v, BLOCKING_DIRECT);
								exp->add_var(var_id);
							}
						}
					}
				}

				if (exp->has_terms())
				{
					if (preemptive)
					{
						unsigned int var_id = vars.lookup_max_preemptions(*resource);
						exp->sub_var(var_id);
					}
					lp.add_inequality(exp, niql);
				}
				else
					delete exp;
			}
		}
	}
}


// Constraint 18:  Limit arrival-blocking due to higher-priority requests
// using the wait-time bound.
bool add_prio_fifo_arrival_blocking_HP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		unsigned int min_prioLP = get_min_prio(info, ti, *resource, true);
		unsigned int ncs = count_local_hp_reqs(info, ti, *resource); //number of req.s to resource from Ti or local higher-prio tasks
		long wait_time_bound = bound_wait_time_prio_fifo(info, ti, *resource, min_prioLP);


		if (wait_time_bound < 0) // use response time if wait-time cannot be bounded
			wait_time_bound = ti.get_response();;

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (ti.get_cluster() != c) // ignore Ti's cluster
			{
				foreach(clusters[c], task)
				{
					LinearExpression *exp = new LinearExpression();
					unsigned int max_num_reqs = 0;
					foreach((*task)->get_requests(), request)
					{
						if ((*request).get_resource_id() == *resource &&
							(*request).get_request_priority() < min_prioLP)
						{
							max_num_reqs += request->get_max_num_requests(wait_time_bound);
							foreach_request_instance(*request, ti, v)
							{
								unsigned int var_id = vars.lookup((*task)->get_id(), (*request).get_resource_id(), v, BLOCKING_ARRIVAL);
								exp->add_var(var_id);
							}
						}
					}

					if (exp->has_terms())
					{
						unsigned int var_id = vars.lookup_arrival_enabled(*resource);
						exp->sub_term(max_num_reqs * ncs, var_id);
						lp.add_inequality(exp, 0);
					}
					else
						delete exp;
				}
			}
		}
	}
	return true;
}


// Constraint 19: limit arrival blocking to one request of same locking priority
// per processor per resource, unless disallowed according to definition of A_q.
void add_prio_fifo_atmostonce_remote_arrival_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp)
{
	std::set<unsigned int> all_resources = get_all_resources(info);
	Clusters clusters;
	split_by_cluster(info, clusters);

	foreach(all_resources, resource)
	{
		unsigned int min_prioLP = get_min_prio(info, ti, *resource, true);

		for (unsigned int c=0; c < clusters.size(); c++)
		{
			if (c != ti.get_cluster())
			{
				LinearExpression *exp = new LinearExpression();
				foreach(clusters[c], task)
				{
					foreach((*task)->get_requests(), request)
					{
						if (request->get_resource_id() == *resource &&
							(*request).get_request_priority() == min_prioLP)
						{
							foreach_request_instance(*request, ti, v)
							{
								unsigned int var_id = vars.lookup((*task)->get_id(), *resource, v, BLOCKING_ARRIVAL);
								exp->add_var(var_id);
							}
						}
					}
				}
				if (exp->has_terms())
				{
					unsigned int var_id = vars.lookup_arrival_enabled(*resource);
					exp->sub_var(var_id);
					lp.declare_variable_binary(var_id);
					lp.add_inequality(exp, 0);
				}
				else
					delete exp;
			}
		}
	}
}


bool add_prio_fifo_constraints(
	VarMapperSpinlocks& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	bool preemptive)
{
	add_common_spinlock_constraints(vars, info, ti, lp);

	if (preemptive)
	{
		add_common_preemptive_spinlock_constraints(vars, info, ti, lp);
		add_preemptive_fifo_max_preempt_constraints(vars, info, ti, lp);
	}

	// Constraint 13, Constraint 14
	add_prio_blocking_LP_constraints(vars, info, ti, lp, preemptive);

	// Constraint 16
	add_prio_fifo_direct_blocking_HP_constraints(vars, info, ti, lp, preemptive);

	// Constraint 17
	add_prio_fifo_max_direct_blocking_constraints(vars, info, ti, lp, preemptive);

	if (!preemptive)
	{
		// Constraint 18
		add_prio_fifo_arrival_blocking_HP_constraints(vars, info, ti, lp);

		// Constraint 19
		add_prio_fifo_atmostonce_remote_arrival_constraints(vars, info, ti, lp);
	}

	return true;
}


bool apply_prio_fifo_bounds_for_task(
	unsigned int i,
	BlockingBounds& bounds,
	const ResourceSharingInfo& info,
	bool preemptive)
{
	LinearProgram lp;
	VarMapperSpinlocks vars;
	const TaskInfo& ti = info.get_tasks()[i];

	add_prio_fifo_constraints(vars, info, ti, lp, preemptive);

	set_spinlock_blocking_objective(vars, info, ti, lp);
	vars.seal();
	Solution *sol = linprog_solve(lp, vars.get_num_vars());

	assert(sol != NULL);
	Interference total;
	total.total_length = lrint(sol->evaluate(*lp.get_objective()));
	bounds[i] = total;

	delete sol;
	return true;
}


BlockingBounds* lp_pfp_prio_fifo_spinlock_bounds(const ResourceSharingInfo& info, bool preemptive)
{
	BlockingBounds* results = new BlockingBounds(info);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		apply_prio_fifo_bounds_for_task(i, *results, info, preemptive);
	}

	return results;
}
