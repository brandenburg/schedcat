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

std::string GlobalVarMapper::key2str(uint64_t key, unsigned int var) const
{
	lookup_key_t k;
	std::ostringstream buf;

	k.raw = key;

	if (k.var.variable_type == INTERFERENCE_BOUND)
	{
		buf << "I";
		switch (k.var.blocking_type)
		{
			case REGULAR_INTERFERENCE:
				buf << "r";
				break;
			case CO_BOOSTING_INTERFERENCE:
				buf << "c";
				break;
			case STALLING_INTERFERENCE:
				buf << "s";
				break;
		}
		buf << "["
			<< k.var.tid << "]";

	}
	else
	{
		buf << "X";
		switch (k.var.blocking_type)
		{
			case DIRECT_BLOCKING:
				buf << "d";
				break;
			case INDIRECT_BLOCKING:
				buf << "i";
				break;
			case PREEMPTION_BLOCKING:
				buf << "p";
				break;
			case EXPELLING_BLOCKING:
				buf << "e";
				break;
		}

		buf << "["
			<< k.var.tid << ", "
			<< k.var.rid << ", "
			<< k.var.xid << "]";
	}

	return buf.str();
}

const double GlobalSuspensionAwareLP::EPSILON = 1e-6;

GlobalSuspensionAwareLP::GlobalSuspensionAwareLP(
	const ResourceSharingInfo& info,
	unsigned int task_index,
	unsigned int number_of_cpus)
	: i(task_index),
	  ti(info.get_tasks()[i]),
	  taskset(info.get_tasks()),
	  m(number_of_cpus),
	  all_resources(get_all_resources(info))
{
	// Code based on this class uses index of a task,
	// it's assigned ID, and its priority interchangeably.
	// Make sure the TaskInfo representation actually
	// satisfies this assumption.
	for (unsigned int j = 0; j < taskset.size(); j++)
	{
		assert(info.get_tasks()[j].get_id() == j);
		assert(info.get_tasks()[j].get_priority() == j);
	}

	set_objective();

	// Constraint 1
	add_workload_constraints();
	// Constraint 2
	add_slack_constraints();
	// Constraint 3
	add_generic_mutex_pi_blocking_constraints();
	// Constraint 4
	add_stalling_interference_for_independent_tasks();
	// Constraint 5
	add_generic_non_access_direct_constraints();

	declare_interference_variables();

	vars.seal(); // every possible variable should have been referenced
}

// declare the interference variables to be lower-bounded by zero
// and not bounded from above
void GlobalSuspensionAwareLP::declare_interference_variables()
{
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();
		this->declare_variable_bounds(vars.regular_interference(tx_id),
									true, 0, // lower bound: 0
									false, -1); // no upper bound
		this->declare_variable_bounds(vars.co_boosting_interference(tx_id),
									true, 0, // lower bound: 0
									false, -1); // no upper bound
		this->declare_variable_bounds(vars.stalling_interference(tx_id),
									true, 0, // lower bound: 0
									false, -1); // no upper bound
	}
}

unsigned long GlobalSuspensionAwareLP::solve_debug()
{
	Solution *sol;
	double result;

	add_constraints_post_ctor();

	hashmap<unsigned int, std::string> var_map;

	var_map = vars.get_translation_table();

	std::cout << std::endl
		<< "=====================================================" << std::endl;
	std::cout << "LP for " << ti << ":" << std::endl;
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

	delete sol;

	assert(result < ULONG_MAX);
	return (unsigned long) result;
}

unsigned long GlobalSuspensionAwareLP::solve()
{
	Solution *sol;

	add_constraints_post_ctor();

	sol = linprog_solve(*this, vars.get_num_vars());

	if (sol != NULL) // Do we have a solution to the LP?
	{
		double result;
		//Get the pi-blocking (including pi-blocking caused by higher-priority tasks)
		result = sol->evaluate(*get_objective());
		delete sol;

		assert(ti.get_response() >= ti.get_cost());
		unsigned long assumed_interference = ti.get_response() - ti.get_cost();

		// deal with floating point imprecision
		if ((result < assumed_interference)
			&& (assumed_interference - result < EPSILON))
		{
			// Result is very close to the response-time estimate, but
			// floating point noise has pushed it just below
			// the assumed response time.

			// Correct the noise by returning the expected interference.
			return assumed_interference;
		}
		else
		{
			result = floor(result);
			assert(result < ULONG_MAX);
			return (unsigned long) result;
		}
	}
	else // No solution to the LP, return ULONG_MAX to indicate that.
		return ULONG_MAX;
}


//-------------------------------
// Objective function and generic constraints:

//objective function
void GlobalSuspensionAwareLP::set_objective()
{
	LinearExpression *obj = get_objective();

	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();

		//add regular interference for higher-base-priority tasks
		if (x < ti.get_id())
		{
			obj->add_term(1.0/m, vars.regular_interference(x));
		}

		//add co-boosting and stalling interference for lower-base-priority tasks
		if (x > ti.get_id())
		{
			obj->add_term(1.0/m, vars.co_boosting_interference(x));
			obj->add_term(1.0/m, vars.stalling_interference(x));
		}


		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			const double length = request->get_request_length();

			foreach_request_instance(*request, ti, v)
			{
				obj->add_term(length, vars.direct(x, q, v));

				// For lower-priority tasks, indirect and preemption
				// pi-blocking are divided by m (number of cpus).
				// Higher-priority tasks do not cause indirect or preemption
				// pi-blocking.
				if (x > ti.get_id())
				{
					const double scaled_length = length / m;

					obj->add_term(scaled_length, vars.indirect(x, q, v));
					obj->add_term(scaled_length, vars.preemption(x, q, v));
				}
			}
		}
	}
}

// Constraint 1: pi-blocking and interference (of any type) due to task Tx
// are bounded by the workload of Tx
void GlobalSuspensionAwareLP::add_workload_constraints()
{
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int tx_id = tx->get_id();

		LinearExpression *exp = new LinearExpression();

		if (tx_id < ti.get_id())	// regular interference of higher-base-priority tasks
			exp->add_var(vars.regular_interference(tx_id));
		else	// co-boosting and stalling interference of lower-base-priority tasks
		{
			exp->add_var(vars.co_boosting_interference(tx_id));
			exp->add_var(vars.stalling_interference(tx_id));
		}

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();
			const unsigned int csl = request->get_request_length();
			// direct blocking of all tasks
			foreach_request_instance(*request, ti, v)
				exp->add_term(csl, vars.direct(tx_id, q, v));

			// indirect and preemption blocking of lower-base-priority tasks
			if (tx_id > ti.get_id())
			{
				foreach_request_instance(*request, ti, v)
				{
					exp->add_term(csl, vars.indirect(tx_id, q, v));
					exp->add_term(csl, vars.preemption(tx_id, q, v));
				}
			}
		}

		add_inequality(exp, tx->workload_bound(ti.get_response()));
	}
}

// Constraint 2: indirect blocking, preemption blocking, and all types of
// interference take place only during the interval while Ji is pending,
// not scheduled and not incurs direct blocking
void GlobalSuspensionAwareLP::add_slack_constraints()
{
	//In the implementation, the inequation in Constraint 2
	//is rearranged such that the items in the
	//RHS is subtracted in the LHS ( then we have "X + Y + Z.... <= 0")
	const double m_inv = 1.0/m;

	foreach_task_except(taskset, ti, tx)
	{
		LinearExpression *exp = new LinearExpression();

		const unsigned int tx_id = tx->get_id();

		if (tx_id < ti.get_id())
			// add (1-1/m)*I_x^R here.
			exp->add_term(1 - m_inv, vars.regular_interference(tx_id));
		else
		{
			// add (1-1/m)*I_x^C and (1-1/m)*I_x^S here.
			exp->add_term(1 - m_inv, vars.co_boosting_interference(tx_id));
			exp->add_term(1 - m_inv, vars.stalling_interference(tx_id));

			// add (1-1/m)*B_x^I and (1-1/m)*B_x^P here.
			foreach(tx->get_requests(), request)
			{
				const unsigned int q = request->get_resource_id();
				const unsigned int csl = request->get_request_length();
				foreach_request_instance(*request, ti, v)
				{
					exp->add_term((1 - m_inv) * csl, vars.indirect(tx_id, q, v));
					exp->add_term((1 - m_inv) * csl, vars.preemption(tx_id, q, v));
				}
			}

		}

		foreach_task_except(taskset, ti, ty)
		{
			const unsigned int ty_id = ty->get_id();

			if (tx_id == ty_id)
				continue;

			// add - 1/m I_y_R in the left hand side of the inequation
			if (ty_id < ti.get_id())
				exp->sub_term(m_inv, vars.regular_interference(ty_id));

			// add - 1/m I_y_C, - 1/m I_y_S, - 1/m B_y_I, and - 1/m B_y_P
			// in the left hand side
			if (ty_id > ti.get_id())
			{
				exp->sub_term(m_inv, vars.co_boosting_interference(ty_id));
				exp->sub_term(m_inv, vars.stalling_interference(ty_id));

				foreach(ty->get_requests(), request)
				{
					const unsigned int q = request->get_resource_id();
					const unsigned int csl = request->get_request_length();
					foreach_request_instance(*request, ti, v)
					{
						exp->sub_term(m_inv * csl, vars.indirect(ty_id, q, v));
						exp->sub_term(m_inv * csl, vars.preemption(ty_id, q, v));
					}
				}
			}
		}

		add_inequality(exp, 0);
	}
}

// Constraint 3: direct, indirect, and preemption blockings are mutually exclusive
void GlobalSuspensionAwareLP::add_generic_mutex_pi_blocking_constraints()
{
	foreach_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();

		foreach(tx->get_requests(), request)
		{
			const unsigned int q = request->get_resource_id();

			foreach_request_instance(*request, ti, v)
			{

				LinearExpression *exp = new LinearExpression();
				exp->add_var(vars.direct(x, q, v));

				// only lower-base-priority tasks cause indirect and preemption blocking
				if (x > ti.get_id())
				{
					exp->add_var(vars.indirect(x, q, v));
					exp->add_var(vars.preemption(x, q, v));
				}

				add_inequality(exp, 1);
			}
		}
	}
}

// Constraint 4: tasks that do not access any resource will not incur
// any stalling interference (for no-progress and the PPCP)
void GlobalSuspensionAwareLP::add_stalling_interference_for_independent_tasks()
{
	unsigned int num_req = ti.get_total_num_requests();

	// no stalling interference if N_iq = 0
	if (num_req == 0)
	{
		LinearExpression *exp = new LinearExpression();

		foreach_lower_priority_task(taskset, ti, tx)
		{
			const unsigned int tx_id = tx->get_id();
			exp->add_var(vars.stalling_interference(tx_id));
		}

		add_inequality(exp, 0);
	}
}

// Constraint 5: a job will not be directly delayed due to resources that
// it does not require
void GlobalSuspensionAwareLP::add_generic_non_access_direct_constraints()
{
	foreach(all_resources, resource)
	{
		bool used = false;

		//check whether ti uses this resource
		foreach(ti.get_requests(), request)
		{
			//set true if ti uses this resource
			if (request->get_resource_id() == *resource)
			{
				used = true;
				break;
			}
		}

		if(!used)	//ti does not use this resource
		{
			LinearExpression *exp = new LinearExpression();

			foreach_task_except(taskset, ti, tx)
			{
				const unsigned int x = tx->get_id();

				foreach(tx->get_requests(), request)
				{
					if (request->get_resource_id() == *resource)
					{
						const unsigned int q = *resource;

						foreach_request_instance(*request, ti, v)
						{
							exp->add_var(vars.direct(x, q, v));
						}
					}
				}
			}

			add_inequality(exp, 0);
		}
	}
}

