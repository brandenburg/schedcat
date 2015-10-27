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

#include "lp_global.h"


class GlobalPPCPAnalysis : public GlobalPrioInheritanceLP, public GlobalPriorityQueuesLP
{

private:

	// Constraint 27
	void add_ppcp_stalling_interference();

	// Constraint 28
	void add_ppcp_total_stalling_interference();

	// Constraint 29
	void add_ppcp_no_stalling_interference();

	// Constraint 30 is the same as Constraint 12

	// Constraint 31
	void add_ppcp_beta_constraints();

	unsigned long compute_beta(unsigned int tl_id);
	unsigned int N_i_l_q_prime(
		unsigned long R_i_prime, unsigned int tl_id, unsigned int q);


public:
	GlobalPPCPAnalysis(const ResourceSharingInfo& info,
					unsigned int i,
					unsigned int number_of_cpus,
					bool reasonable_priority_assignment)
		: GlobalSuspensionAwareLP(info, i, number_of_cpus),
		  GlobalPrioInheritanceLP(info, i, number_of_cpus),
		  GlobalPriorityQueuesLP(info, i, number_of_cpus)
	{
		// Protocol-specific constraints
		// Constraint 27
		add_ppcp_stalling_interference();
		// Constraint 28
		add_ppcp_total_stalling_interference();
		// Constraint 29
		add_ppcp_no_stalling_interference();
		// Constraint 30 (the same as Constraint 12 in the paper)
		add_pip_ppcp_indirect_preemption_constraints();

		// Constraint 31
		// NOTE: currently it only applies under the "reasonable priority assignment",
		// as noted in the origial RTSS'09 paper
		if (reasonable_priority_assignment)
			add_ppcp_beta_constraints();
	}
};


// Constraint 27: the stalling interference that Ji (i<m) incurs due to Tx is limited
// to the sum of the multiple of the total number of requests to resource \res_q by Ji
// and the largest (m-1) values in \Phi_iq
void GlobalPPCPAnalysis::add_ppcp_stalling_interference()
{
	unsigned int total = 0;

	std::vector<unsigned int> theta_iq_per_task;

	// Constraint only applies if i >= m.
	if (ti.get_id() < m)
		return;

	foreach(all_resources, resource)
	{
		unsigned int res_id = *resource;
		unsigned int num_of_requests = ti.get_num_requests(res_id);

		// if Ti does not access this resource,
		// then continue and check the other resources
		if (num_of_requests == 0)
			continue;

		// theta_iq_per_task is used to for \Phi_iq in the paper
		theta_iq_per_task.clear();

		//fill up the values of \Phi_iq
		foreach_lower_priority_task(taskset, ti, tx)
		{
			unsigned long max = 0;
			foreach(tx->get_requests(), request)
			{
				// priority ceiling higher than the base priority of Ti
				// and not resource with id res_id
				if ((prio_ceilings.at(res_id) < ti.get_id()) && (res_id != request->get_resource_id()))
				{
					unsigned long csl = request->get_request_length();

					if (csl > max)
						max = csl;
				}
			}
			theta_iq_per_task.push_back(max);
		}

		std::sort(theta_iq_per_task.begin(), theta_iq_per_task.end());

		unsigned long theta_iq = 0;

		//sum up the m-1 largest values
		for (unsigned int j=0; j < m-1; j++)
		{
			if (theta_iq_per_task.empty())
				break;
			theta_iq += theta_iq_per_task.back();
			theta_iq_per_task.pop_back();
		}

		total += theta_iq * num_of_requests;
	}

	foreach_lower_priority_task(taskset, ti, tx)
	{
		LinearExpression *exp = new LinearExpression();

		const unsigned int tx_id = tx->get_id();

		exp->add_var(vars.stalling_interference(tx_id));

		if (total > ti.get_deadline())
			total = ti.get_deadline();

		add_inequality(exp, total);
	}
}

// Constraint 28: in addition to Constraint 27, this constraint limit the
// total cumulative stalling interference due to all lower-base-priority tasks
void GlobalPPCPAnalysis::add_ppcp_total_stalling_interference()
{
	std::vector<unsigned int> theta_iq_per_task;

	unsigned int total = 0;


	// Constraint only applies if i>m.
	if (ti.get_id() < m)
		return;

	foreach(all_resources, resource)
	{
		unsigned int res_id = *resource;
		unsigned int num_of_requests = ti.get_num_requests(res_id);

		// if Ti does not access this resource,
		// then continue and check the other resources
		if (num_of_requests == 0)
			continue;

		theta_iq_per_task.clear();

		foreach_lower_priority_task(taskset, ti, tx)
		{
			unsigned int max = 0;
			foreach(tx->get_requests(), request)
			{
				// priority ceiling higher than the base priority of Ti
				// and not resource with id res_id
				if ((prio_ceilings.at(res_id) < ti.get_id()) && (res_id != request->get_resource_id()))
				{
					unsigned long csl = request->get_request_length();

					if (csl > max)
						max = csl;
				}
			}
			theta_iq_per_task.push_back(max);
		}

		std::sort(theta_iq_per_task.begin(), theta_iq_per_task.end());

		unsigned long theta_iq = 0;

		for (unsigned int j=m; j > 0; j--)
		{
			if (theta_iq_per_task.empty())
				break;
			theta_iq += theta_iq_per_task.back() * j;
			theta_iq_per_task.pop_back();
		}

		total += theta_iq * num_of_requests;
	}

	LinearExpression *exp = new LinearExpression();
	foreach_lower_priority_task(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();
		exp->add_var(vars.stalling_interference(tx_id));
	}

	if (total > ti.get_deadline())
		total = ti.get_deadline();

	add_inequality(exp, total);
}

// Constraint 29: rule out stalling interference of each lower-base-priority
// task Tx if Tx and all of Tx's lower-base-priority tasks do not access any resource
void GlobalPPCPAnalysis::add_ppcp_no_stalling_interference()
{
	unsigned int lowest_task_w_reqs = 0; //initialized with highest priority

	// Among the lower-prio tasks, find the one with lowest prio
	// that accesses a resource.
	foreach_lower_priority_task(taskset, ti, tx)
	{
		if ((tx->get_total_num_requests() > 0) && (tx->get_id() > lowest_task_w_reqs))
			 lowest_task_w_reqs = tx->get_id();
	}

	// Among the lower-prio tasks, lowest_task_w_reqs is the ID of the
	// task with lowest ID that accesses any resource. If no such task exists
	// (no lower-prio tasks accesses a resource), lowest_task_w_reqs still
	// has the initialized value of 0.
	foreach_lower_priority_task(taskset, ti, tx)
	{
		// all tasks with id > index do not access any resource
		// notably, just ">" not ">="
		if (tx->get_id() > lowest_task_w_reqs)
		{
			LinearExpression *exp = new LinearExpression();
			exp->add_var(vars.stalling_interference(tx->get_id()));
			add_inequality(exp, 0);
		}
	}
}

// Constraint 30: the same as Constraint 12

std::set<unsigned int> compute_sr_i_l_prime(const TaskInfo *ti,
											const TaskInfo *tl,
											const PriorityCeilings &pc)
{
	std::set<unsigned int> sr_i_l_prime;

	foreach (tl->get_requests(), request)
	{
		unsigned int res_id = request->get_resource_id();
		if (pc.at(res_id) < ti->get_id())
			sr_i_l_prime.insert(res_id);
	}
	return sr_i_l_prime;
}


unsigned long compute_e_i_l_prime(const TaskInfo *ti,
								  const TaskInfo *tl,
								  const PriorityCeilings &pc)
{
	unsigned long e_i_l_prime = 0;
	std::set<unsigned int> sr_i_l_prime = compute_sr_i_l_prime(ti, tl, pc);

	foreach (tl->get_requests(), request)
	{
		unsigned int res_id = request->get_resource_id();
		if (sr_i_l_prime.find(res_id) != sr_i_l_prime.end())
		{
			e_i_l_prime += request->get_request_length() * request->get_num_requests();
		}
	}

	return e_i_l_prime;
}


unsigned long GlobalPPCPAnalysis::compute_beta(unsigned int tl_id)
{
	const TaskInfo *ti = &taskset[i];
	const TaskInfo *tl = &taskset[tl_id];
	assert(ti && tl);
	unsigned long e_i_l_prime = compute_e_i_l_prime(ti, tl, prio_ceilings);

	unsigned long beta;

	if ( (ti->get_response()) > (tl->get_period() - tl->get_response() + 2*e_i_l_prime) )
	{
		beta = ti->get_response()
			 + tl->get_response()
			 - tl->get_period()
			 - 2*e_i_l_prime;
	}
	else if ( (ti->get_response() > e_i_l_prime) && (ti->get_response() <= tl->get_period() - tl->get_response() + e_i_l_prime) )
	{
		beta = ti->get_response() - e_i_l_prime;
	}
	else
		beta = 0;

	return beta;
}


unsigned int N_i_l_prime(unsigned long R_i_prime, const TaskInfo *tl)
{
	return std::ceil((R_i_prime + tl->get_response()) / (1.0 * tl->get_period()));
}

unsigned int GlobalPPCPAnalysis::N_i_l_q_prime(unsigned long R_i_prime, unsigned int tl_id, unsigned int q)
{
	const TaskInfo *tl = &taskset[tl_id];
	unsigned int reqs_for_q = 0;
	foreach(tl->get_requests(), request)
	{
		if (request->get_resource_id() == q)
		{
			reqs_for_q += request->get_num_requests();
		}
	}
	return N_i_l_prime(R_i_prime, tl) * reqs_for_q;
}


struct ppcp_beta_comparator
{
	std::map<unsigned int, unsigned long> beta_map;
	bool operator() (int i,int j)
	{
		return (beta_map[i] < beta_map[j]);
	}
};


// Constraint 31
void GlobalPPCPAnalysis::add_ppcp_beta_constraints()
{
	const PriorityCeilings &pc = prio_ceilings;

	// We first compute the beta values of all tasks except for Ti.
	struct ppcp_beta_comparator beta_comparator;
	std::vector<unsigned int> lower_prio_task_ids;
	foreach_lower_priority_task(taskset, ti, tx)
	{
			unsigned long beta = compute_beta(tx->get_id());
			beta_comparator.beta_map.insert(std::pair<unsigned int, unsigned long>(tx->get_id(), beta));
			lower_prio_task_ids.push_back(tx->get_id());
	}

	// We sort the tasks according to the beta values.
	std::sort(lower_prio_task_ids.begin(), lower_prio_task_ids.end(), beta_comparator);

	// To compute the gamma set, we take the first m tasks, ie, the ones
	// with the smalles beta values.
	std::set<unsigned int> gamma;
	for (std::vector<unsigned int>::iterator it = lower_prio_task_ids.begin();
		 it != lower_prio_task_ids.end(); it++)
	{
		gamma.insert(*it);
		if (gamma.size() >= m)
			break;
	}

	// compute R_i'
	// For each task, compute the cumulative CSL of requests to
	// resources with a prio ceiling higher than Ti's priority.
	// Put all of these values into one set.
	std::set<unsigned long> min_candidates;
	foreach_lower_priority_task(taskset, ti, tx)
	{
		unsigned long total_pc_req_length = 0;
		foreach(tx->get_requests(), request)
		{
			if (pc.at(request->get_resource_id()) < ti.get_id())
				total_pc_req_length += request->get_num_requests() * request->get_request_length();
		}
		min_candidates.insert(total_pc_req_length);
	}

	// If no cum. CSL was added, add a 0 to make sure that there is a value.
	if (min_candidates.size() <= 0)
		min_candidates.insert(0);

	// Get the minimum cum. CSL.
	unsigned long min_total_pc_req_length = *(std::min_element(min_candidates.begin(), min_candidates.end()));
	unsigned long R_i_prime = ti.get_response() - min_total_pc_req_length;

	// now we can finally establish the constraint..

	foreach_lower_priority_task(taskset, ti, tx)
	{
		bool tx_is_in_gamma = (gamma.find(tx->get_id()) != gamma.end());
		if (tx_is_in_gamma)
			continue;

		unsigned int tx_id = tx->get_id();
		foreach(tx->get_requests(), request)
		{
			if (pc.at(request->get_resource_id()) < ti.get_id())
			{
				LinearExpression *exp = new LinearExpression();
				unsigned int res_id = request->get_resource_id();
				foreach_request_instance(*request, ti, v)
				{
					exp->add_var(vars.indirect(tx_id, res_id, v));
					exp->add_var(vars.preemption(tx_id, res_id, v));
				}
				unsigned long rhs = N_i_l_q_prime(R_i_prime, tx_id, res_id);
				add_inequality(exp, rhs);
			}
		}
	}
}

BlockingBounds* lp_ppcp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus,
	bool reasonable_priority_assignment)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		GlobalPPCPAnalysis lp(info, i, number_of_cpus, reasonable_priority_assignment);
		(*results)[i] = lp.solve();
	}

	return results;
}
