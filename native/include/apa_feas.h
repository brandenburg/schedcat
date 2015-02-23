#ifndef APA_FEAS_H
#define APA_FEAS_H

#ifndef SWIG

#include <vector>

#include "stl-hashmap.h"

#endif


typedef hashset<unsigned int> Affinity;
typedef std::vector<Affinity> Affinities;

class AffinityRestrictions
{

private:
	// set of allowed CPUs for each task
	Affinities affinities;

public:

	const Affinities & get_affinities() const
	{
		return affinities;
	}

	void add_cpu(unsigned int task_id, unsigned int allowed_cpu)
	{
		while (affinities.size() <= task_id)
			affinities.push_back(Affinity());

		affinities[task_id].insert(allowed_cpu);
	}

	unsigned int get_task_count() const
	{
		return affinities.size();
	}

	Affinity get_all_cpus() const;
};

class APAFeasibleSolution
{

private:
	// for each task, for each CPU
	std::vector< std::vector<double> > allocation;

public:

	double get_fraction(unsigned int task_id, unsigned int on_cpu) const
	{
		if (allocation.size() <= task_id)
			return 0;

		if (allocation[task_id].size() <= on_cpu)
			return 0;

		return allocation[task_id][on_cpu];
	}

	void set_fraction(unsigned int task_id, unsigned int on_cpu, double frac)
	{
		while (allocation.size() <= task_id)
			allocation.push_back(std::vector<double>());

		while (allocation[task_id].size() <= on_cpu)
			allocation[task_id].push_back(0);

		allocation[task_id][on_cpu] = frac;
	}

};


APAFeasibleSolution* apa_implicit_deadline_feasible(
	const TaskSet &ts, const AffinityRestrictions &taskset_affinities);

#endif
