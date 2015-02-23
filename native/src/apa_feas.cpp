#include <cassert>
#include <climits>

#include <iostream>

#include <stdint.h>

#include "stl-helper.h"
#include "tasks.h"
#include "apa_feas.h"

#include "linprog/varmapperbase.h"
#include "linprog/model.h"
#include "linprog/solver.h"

Affinity AffinityRestrictions::get_all_cpus() const
{
	Affinity all;

	foreach(affinities, af)
	{
		all.insert(af->begin(), af->end());
	}

	return all;
}

class CPUFractionVarMapper : public VarMapperBase {

	union key_val {
		uint64_t raw;
		struct {
			uint32_t cpu;
			uint32_t tsk;
		} var;
	};

public:

	unsigned int fraction(unsigned int task_id, unsigned int cpu)
	{
		key_val k;

		k.var.cpu = cpu;
		k.var.tsk = task_id;

		return var_for_key(k.raw);
	}

};


class APAImplicitDeadlineFeasibilityLP : LinearProgram {

private:
	CPUFractionVarMapper vars;

	const TaskSet &tasks;
	const Affinities &affinities;
	Affinity all_cpus;
	Solution* solution;

	void add_task_service_constraints();
	void add_cpu_capacity_constraints();

	typedef const unsigned int var_t;

public:
	APAImplicitDeadlineFeasibilityLP(
		const TaskSet &ts,
		const AffinityRestrictions &affinity_restrictions);

	~APAImplicitDeadlineFeasibilityLP();

	bool is_feasible() const;

	APAFeasibleSolution* get_solution();
};

APAImplicitDeadlineFeasibilityLP::APAImplicitDeadlineFeasibilityLP(
	const TaskSet &ts,
	const AffinityRestrictions &affinity_restrictions)
	: tasks(ts),
	  affinities(affinity_restrictions.get_affinities())
{
	if (!ts.has_only_implicit_deadlines())
	{
		std::cerr << std::endl << "[!!] "
			<< "Attempted to call implicit-deadline feasibility test "
			<< "on task set that has non-implicit deadlines." << std::endl;
		abort(); // this test applies only to implicit-deadline tasks
	}


	if (!ts.has_no_self_suspending_tasks())
	{
		std::cerr << std::endl << "[!!] "
			<< "Attempted to call APA feasibility test "
			<< "on task set that has self-suspending tasks." << std::endl;
		abort(); // this test applies only to implicit-deadline tasks
	}

	if (ts.get_task_count() != affinity_restrictions.get_task_count())
	{
		std::cerr << std::endl << "[!!] "
			<< "Attempted to call implicit-deadline feasibility test "
			<< "on task set that has non-implicit deadlines." << std::endl;
		abort(); // this test applies only to implicit-deadline tasks
	}

	if (ts.has_only_feasible_tasks())
	{
		all_cpus = affinity_restrictions.get_all_cpus();

		add_task_service_constraints();
		add_cpu_capacity_constraints();
		vars.seal();

		solution = linprog_solve(*this, vars.get_num_vars());
	}
	else
		solution = NULL;
}

APAImplicitDeadlineFeasibilityLP::~APAImplicitDeadlineFeasibilityLP()
{
	delete solution;
}

void APAImplicitDeadlineFeasibilityLP::add_task_service_constraints()
{
	for (unsigned int i = 0; i < tasks.get_task_count(); i++)
	{
		LinearExpression *exp = new LinearExpression();

		foreach(affinities[i], cpu)
		{
			var_t x = vars.fraction(i, *cpu);
			exp->add_var(x);
		}

		add_equality(exp, 1);
	}
}

void APAImplicitDeadlineFeasibilityLP::add_cpu_capacity_constraints()
{
	foreach(all_cpus, cpu)
	{
		LinearExpression *exp = new LinearExpression();
		for (unsigned int i = 0; i < tasks.get_task_count(); i++)
		{
			double util = tasks[i].get_utilization();
			var_t x = vars.fraction(i, *cpu);
			exp->add_term(util, x);
		}
		add_inequality(exp, 1);
	}
}

bool APAImplicitDeadlineFeasibilityLP::is_feasible() const
{
	return solution != NULL;
}

APAFeasibleSolution* APAImplicitDeadlineFeasibilityLP::get_solution()
{
	if (!is_feasible())
		return NULL;

	APAFeasibleSolution* sol = new APAFeasibleSolution();

	for (unsigned int i = 0; i < tasks.get_task_count(); i++)
	{
		foreach(affinities[i], cpu)
		{
			var_t x = vars.fraction(i, *cpu);
			sol->set_fraction(i, *cpu, solution->get_value(x));
		}
	}
	return sol;
}

APAFeasibleSolution* apa_implicit_deadline_feasible(
	const TaskSet &ts, const AffinityRestrictions &affinity_restrictions)
{
	APAImplicitDeadlineFeasibilityLP lp(ts, affinity_restrictions);

	return lp.get_solution();
}
