#include <assert.h>
#include <limits.h>
#include <cmath>

#include <algorithm>
#include <iterator>

#include "linprog/model.h"
#include "linprog/varmapperbase.h"
#include "linprog/solver.h"

#include "sharedres_types.h"
#include "iter-helper.h"

#include "stl-hashmap.h"
#include "stl-helper.h"
#include "stl-io-helper.h"

#include "nested_cs.h"

#include <iostream>
#include <sstream>
#include "res_io.h"
#include "linprog/io.h"

void dump(const CriticalSectionsOfTaskset &x)
{
	int i;

	enumerate(x.get_tasks(), tsk, i)
	{
		std::cout << "Tsk: " << i << ": ";

		foreach (tsk->get_cs(), cs)
		{
			std::cout << "(" << cs->resource_id
					  << ", " << cs->length;
			if (cs->is_nested())
			{
				std::cout << ", " << cs->outer;
			}
			std::cout << ") ";
		}

		std::cout << std::endl;
	}

	hashmap<unsigned int, hashset<unsigned int> > trans_nested =
		x.get_transitive_nesting_relationship();

	foreach(trans_nested, it)
		std::cout << "R" << it->first << " contains: " << it->second << std::endl;
}

/* Old version of g++/Linux workaround: it doesn't know UINT64_MAX even when
 * include stdint.h...
 */
#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t) 0xffffffffffffffffULL)
#endif

class NestedVarMapper : public VarMapperBase
{
	union key_val
	{
		uint64_t raw;
		struct
		{
			/* 1st vertex */
			uint64_t v1_tid:10; // task ID
			uint64_t v1_rid:10; // resource ID
			uint64_t v1_xid:10; // request ID

			/* 2nd vertex, used only for edges */
			uint64_t v2_tid:10; // task ID
			uint64_t v2_rid:10; // resource ID
			uint64_t v2_xid:10; // request ID

			/* vertex tag: connected by (direct) mutex or nesting edge? */
			uint64_t dn_tag:1;
		} var;


		/* we define all bits set to mean "source vertex" (if 1st vertex) or
		 * "unused" (=not an edge, if 2nd vertex) */
		enum
		{
			KEY_MAX = (unsigned) (1 << 10) - 1,
			ROOT    = (unsigned) (1 << 10) - 1,
		};

		/* construct an X variable (= variable for a vertex) */
		void vertex(unsigned int task_id, unsigned int res_id, unsigned int cs_id)
		{
			/* don't use this in the hacked-up version */
			abort();
			assert(task_id < KEY_MAX);
			assert(res_id < KEY_MAX);
			assert(cs_id < KEY_MAX);

			raw = UINT64_MAX;
			var.v1_tid = task_id;
			var.v1_rid = res_id;
			var.v1_xid = cs_id;
		}

		/* construct an X variable (= variable for a vertex) */
		void vertex(unsigned int task_id, unsigned int res_id, unsigned int cs_id,
			    bool direct)
		{
			/* don't use this in the hacked-up version */
			assert(task_id < KEY_MAX);
			assert(res_id < KEY_MAX);
			assert(cs_id < KEY_MAX);

			raw = UINT64_MAX;
			var.v1_tid = task_id;
			var.v1_rid = res_id;
			var.v1_xid = cs_id;

			var.dn_tag = direct;
		}
	};

	static const key_val vertex_val;

public:
	unsigned int vertex(unsigned int task_id, unsigned int res_id, unsigned int cs_id)
	{
		key_val k;

		k.vertex(task_id, res_id, cs_id);
		return var_for_key(k.raw);
	}

	unsigned int vertex_direct(
		unsigned int task_id,
		unsigned int res_id,
		unsigned int cs_id)
	{
		key_val k;

		k.vertex(task_id, res_id, cs_id, true);
		return var_for_key(k.raw);
	}

	unsigned int vertex_nested(
		unsigned int task_id,
		unsigned int res_id,
		unsigned int cs_id)
	{
		key_val k;

		k.vertex(task_id, res_id, cs_id, false);
		return var_for_key(k.raw);
	}

	std::string key2str(uint64_t key, unsigned int var) const;
};


std::string NestedVarMapper::key2str(uint64_t key, unsigned int var) const
{
	key_val k;
	std::ostringstream buf;

	k.raw = key;

	if (k.var.v2_tid == key_val::ROOT &&
		k.var.v2_rid == key_val::ROOT &&
		k.var.v2_xid == key_val::ROOT)
	{
		// vertex
		buf << "X" << (k.var.dn_tag ? "d" : "n") << "["
			<< k.var.v1_tid << "," << k.var.v1_rid << "," << k.var.v1_xid << "]";
	}
	else
	{
		// unknown type of variable...
		buf << "?["
		    << k.var.v1_tid << "," << k.var.v1_rid << "," << k.var.v1_xid
		    << ";"
		    << k.var.v2_tid << "," << k.var.v2_rid << "," << k.var.v2_xid
		    << "]";
	}
	return buf.str();
}


class NestedFifoILP : protected LinearProgram
{
	NestedVarMapper vars;

	const int i;
	const TaskInfo& ti;
	const CriticalSectionsOfTask& csi;
	const TaskInfos& taskset;
	const CriticalSectionsOfTasks& taskset_cs;

	unsigned int max_cpu; // number of CPUs, discovered from taskset
	unsigned int max_resource; // max resource ID, auto-discovered

	// for each resource, the set of processors from which it is accessed
	hashmap<unsigned int, std::set<unsigned int> > accessed_from;

	// for each resource, the priority ceiling (max. client prio)
	hashmap<unsigned int, unsigned int> prio_ceiling;

	// for each task, for each critical section, the set of outer locks
	std::vector<std::vector<LockSet> > outer_locks;

	std::set<LockSet> serialization_lock_sets;

	// For each resource 'q', the set of resources that are guaranteed
	// to have been visited on any path from the source vertex to any
	// remote request for 'q' delaying Ti. As this set is valid for any
	// possible path, it
	// 1) is necessarily conservative, and
	// 2) useful to rule out impossible mutex edges without actually
	//    exploring all paths.
	std::vector<LockSet> guaranteed_held_on_path;
	// The same idea, just on a per critical section basis.
	std::vector<std::vector<LockSet> > guaranteed_held_cs_path;

	void raise_prio_ceiling(unsigned int res_q, unsigned int prio)
	{
		if (prio_ceiling.find(res_q) == prio_ceiling.end())
			prio_ceiling[res_q] = prio;
		else
			prio_ceiling[res_q] = std::min(prio, prio_ceiling[res_q]);
	}

	void record_access(unsigned int res_q, unsigned int from_cpu)
	{
		if (accessed_from.find(res_q) == accessed_from.end())
			accessed_from[res_q] = std::set<unsigned int>();
		accessed_from[res_q].insert(from_cpu);
	}

	bool is_accessed_from(unsigned int res_q, unsigned int from_cpu)
	{
		return accessed_from[res_q].find(from_cpu) !=
			accessed_from[res_q].end();
	}

	bool is_local_resource_with_lower_prio_ceiling(unsigned int res_q)
	{
		return is_accessed_from(res_q, ti.get_cluster())
			&& accessed_from[res_q].size() == 1
			&& prio_ceiling[res_q] > ti.get_priority();
	}

	void precompute_helper_sets();
	void determine_implicit_serialization_lock_sets();
	void precompute_guaranteed_held();
	void update_guaranteed_lock_set(
		unsigned int x, unsigned int cs_index, LockSet &guaranteed);

	unsigned int num_jobs_to_consider(const TaskInfo& tx)
	{
		if (tx.get_cluster() != ti.get_cluster())
		{
			// standard formula for remote tasks
			return tx.get_max_num_jobs(ti.get_response());
		}
		else if (tx.get_priority() < ti.get_priority())
		{
			// uniprocessor bound on max # of preemptions
			return tx.uni_fp_local_get_max_num_jobs(ti.get_response());
		}
		else
			// local & lower or equal priority => only one job is relevant
			return 1;
	}

	void set_objective();

	void add_type_constraints();
	void add_nesting_constraints();

	void add_local_resource_constraints();
	void add_arrival_blocking_constraints();

	void add_remote_blocking_constraints();
	void add_remote_blocking_constraints_for_core(unsigned int k);
	void add_remote_blocking_constraints_for_resource(
		unsigned int k, unsigned int q, const LockSet& serializing);

public:

	NestedFifoILP(
		const ResourceSharingInfo& tsk,
		const CriticalSectionsOfTaskset& tsk_cs,
		const int task_under_analysis);

	unsigned long solve();
};

NestedFifoILP::NestedFifoILP(
	const ResourceSharingInfo& tsk,
	const CriticalSectionsOfTaskset& tsk_cs,
	int task_under_analysis)
  : i(task_under_analysis),
	ti(tsk.get_tasks()[i]),
	csi(tsk_cs.get_tasks()[i]),
	taskset(tsk.get_tasks()),
	taskset_cs(tsk_cs.get_tasks()),
	max_cpu(0),
	max_resource(0)
{
	precompute_helper_sets();
	precompute_guaranteed_held();

	set_objective();

	add_type_constraints();
	vars.seal();
	add_nesting_constraints();


	add_local_resource_constraints();
	add_arrival_blocking_constraints();

	add_remote_blocking_constraints();

	assert(vars.get_num_vars() == get_binary_variables().size());
}

unsigned long NestedFifoILP::solve()
{
	Solution *sol;
	double result;

	hashmap<unsigned int, std::string> var_map;

	var_map = vars.get_translation_table();

	sol = linprog_solve(*this, vars.get_num_vars());

	result = ceil(sol->evaluate(*get_objective()));

	delete sol;

	assert(result < ULONG_MAX);
	return (unsigned long) result;
}

#define enumerate_cs_instances(_task, _task_cs, _cs_index, _v) \
	for ( \
		unsigned int __limit = \
			num_jobs_to_consider((_task)) * (_task_cs).size(),  \
			_v = _cs_index; \
		(_v) < __limit; \
		(_v) += (_task_cs).size())

typedef const unsigned int var_t;

void NestedFifoILP::set_objective()
{
	unsigned int x;
	LinearExpression *obj;

	obj = get_objective();

	enumerate(taskset, tsk, x)
	{
		/* make sure id and index are in agreement */
		assert( x == tsk->get_id() );

		/* all local lower-priority or remote tasks */
		if (tsk->get_priority() > ti.get_priority() ||
		    tsk->get_cluster() != ti.get_cluster())
		{

			unsigned int cs_index;
			const CriticalSections& tx_cs = taskset_cs[x].get_cs();

			/* for all critical sections of Tx */
			enumerate(tx_cs, cs, cs_index)
			{
				/* for all instances of *cs while a job of Ti is pending */
				enumerate_cs_instances(*tsk, tx_cs, cs_index, v)
				{
					var_t var_d = vars.vertex_direct(x, cs->resource_id, v);
					var_t var_n = vars.vertex_nested(x, cs->resource_id, v);

					obj->add_term(cs->length, var_d);
					obj->add_term(cs->length, var_n);
				}
			}
		}
	}

}

// This is Constraint 3 in the writeup.
void NestedFifoILP::add_type_constraints()
{
	/* for all tasks */
	foreach(taskset, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			const unsigned int q = cs->resource_id;
			/* for all instances of *cs while a job of Ti is pending */
			enumerate_cs_instances(*tx, tx_cs, cs_index, v)
			{
				LinearExpression *exp = new LinearExpression();
				var_t var_d = vars.vertex_direct(x, q, v);
				var_t var_n = vars.vertex_nested(x, q, v);

				declare_variable_binary(var_d);
				declare_variable_binary(var_n);
				exp->add_var(var_d);
				exp->add_var(var_n);
				add_inequality(exp, 1);
			}
		}
	}
}

// This corresponds to Constraints 4 & 5 in the writeup.
void NestedFifoILP::add_nesting_constraints()
{
	/* for all tasks */
	foreach(taskset, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			const unsigned int q = cs->resource_id;

			/* is this a nested request ? */
			if (cs->is_nested())
			{
				// Constraint 4 in the writeup.

				const unsigned int delta = cs_index - cs->outer;
				const CriticalSection &ocs = tx_cs[cs->outer];
				const unsigned int o = ocs.resource_id;

				/* for all instances of *cs while a job of Ti is pending */
				enumerate_cs_instances(*tx, tx_cs, cs_index, v)
				{
					const unsigned int u = v - delta;
					LinearExpression *exp = new LinearExpression();

					/* R_{x,q,v} is nested in R_{x,o,u} */
					var_t var_od = vars.vertex_direct(x, o, u);
					var_t var_on = vars.vertex_nested(x, o, u);
					var_t var_n  = vars.vertex_nested(x, q, v);

					exp->add_var(var_n);
					exp->sub_var(var_od);
					exp->sub_var(var_on);
					add_inequality(exp, 0);
				}
			}
			else
			{
				// Constraint 5 in the writeup.

				/* if it is not nested, disable nesting vars */
				/* for all instances of *cs while a job of Ti is pending */
				enumerate_cs_instances(*tx, tx_cs, cs_index, v)
				{
					LinearExpression *exp = new LinearExpression();
					var_t var_n  = vars.vertex_nested(x, q, v);

					exp->add_var(var_n);
					add_equality(exp, 0);
				}
			}
		}
	}
}


void NestedFifoILP::determine_implicit_serialization_lock_sets()
{
	// Determine suitable test sets.
	// In theory, this could be the power set of the set of all resources.
	// In practice, let's only look at some "interesting" subsets.
	// Note: always include the empty set.

	// TODO: no reason that we need to recompute this for every task...

	LockSet empty_set;
	serialization_lock_sets.insert(empty_set);

	// For now, just consider each resource by itself...
	for (unsigned int q = 0; q < max_resource; q++)
	{
		LockSet singleton;
		singleton.insert(q);
		serialization_lock_sets.insert(singleton);
	}

	// ... and also all outer lock sets.
	foreach(taskset, t)
	{
		unsigned int x   = t->get_id();
		unsigned int ncs = taskset_cs[t->get_id()].get_cs().size();

		for (unsigned int cs_index = 0; cs_index < ncs; cs_index++)
		{
			const LockSet& outer = outer_locks[x][cs_index];
			serialization_lock_sets.insert(outer);
		}
	}
}

// This corresponds to Constraint 6 in the writeup.
void NestedFifoILP::add_remote_blocking_constraints()
{
	for (unsigned int k = 0; k <= max_cpu; k++)
		if (k != ti.get_cluster())
			add_remote_blocking_constraints_for_core(k);
}

void NestedFifoILP::add_remote_blocking_constraints_for_core(unsigned int k)
{
	for (unsigned int q = 0; q <= max_resource; q++)
	{
		// Check which implicitly serializing sets make sense
		// for q.
		foreach(serialization_lock_sets, sr)
		{
			if (sr->empty() || *std::max_element(sr->begin(), sr->end()) < q)
				add_remote_blocking_constraints_for_resource(k, q, *sr);
		}
	}
}

void NestedFifoILP::add_remote_blocking_constraints_for_resource(
	unsigned int k, unsigned int q, const LockSet& serializing)
{
	LinearExpression *exp = new LinearExpression();

	/* (1) enumerate LHS of constraint */
	foreach_task_in_cluster(taskset, k, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			/* is this a request for q? */
			if (cs->resource_id == q)
			{
				/* check that SR is a subset */
				if (is_subset_of(serializing, outer_locks[x][cs_index]))
				{
					/* for all instances of *cs while a job of Ti is pending */
					enumerate_cs_instances(*tx, tx_cs, cs_index, v)
					{
						var_t var_d  = vars.vertex_direct(x, q, v);
						exp->add_var(var_d);
					}
				}
			}
		}
	}


	if (!exp->has_terms())
	{
		/* didn't find any requests for 'q' on processor 'k' => skip */
		delete exp;
		return;
	}

	/* (2) enumerate RHS of constraint, direct blocking */
	foreach_local_task(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			/* is this a request for q? */
			if (cs->resource_id == q)
			{
				if (is_disjoint(serializing, outer_locks[x][cs_index]))
				{
					/* for all instances of *cs while a job of Ti is pending */
					enumerate_cs_instances(*tx, tx_cs, cs_index, v)
					{
						var_t var_d  = vars.vertex_direct(x, q, v);
						exp->sub_var(var_d);
					}
				}
			}
		}
	}

	/* (3) enumerate RHS of constraint, nested blocking */
	foreach_task_not_in_cluster(taskset, k, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			/* is this a request for q? */
			if (cs->resource_id == q)
			{
				if (is_disjoint(serializing, outer_locks[x][cs_index]) &&
					is_disjoint(serializing, guaranteed_held_cs_path[x][cs_index]))
				{
					/* for all instances of *cs while a job of Ti is pending */
					enumerate_cs_instances(*tx, tx_cs, cs_index, v)
					{
						var_t var_n  = vars.vertex_nested(x, q, v);
						exp->sub_var(var_n);
					}
				}
			}
		}
	}


	add_inequality(exp, 0);
}


/* TODO: cache this once for the task set, don't recompute
 * for each task...
 */
void NestedFifoILP::precompute_helper_sets()
{
	outer_locks.reserve(taskset.size());
	guaranteed_held_cs_path.reserve(taskset.size());

	foreach(taskset, t)
	{
		/* discover largest CPU id */
		max_cpu = std::max(max_cpu, t->get_cluster());

		outer_locks.push_back(std::vector<LockSet>());
		std::vector<LockSet> &per_cs = outer_locks.back();

		unsigned int ncs = taskset_cs[t->get_id()].get_cs().size();
		per_cs.reserve(ncs);
		guaranteed_held_cs_path.push_back(std::vector<LockSet>(ncs));

		foreach(taskset_cs[t->get_id()].get_cs(), cs)
		{
			/* discover largest resource id */
			max_resource = std::max(max_resource, cs->resource_id);

			/* record access */
			record_access(cs->resource_id, t->get_cluster());

			/* update priority ceiling */
			raise_prio_ceiling(cs->resource_id, t->get_priority());

			/* record which locks are already held when this CS
			 * executes */
			per_cs.push_back(cs->get_outer_locks(taskset_cs[t->get_id()]));
		}
	}

	determine_implicit_serialization_lock_sets();
}

void NestedFifoILP::precompute_guaranteed_held()
{
	LockSet neutral_element;
	for (unsigned int q = 0; q <= max_resource; q++)
	{
		// Initialize to neutral with copy constructor.
		// => We initially assume the maximal set and then
		//    refine it based on evidence that some resources may not
		//    be included.
		guaranteed_held_on_path.push_back(LockSet(neutral_element));
		LockSet &ghq = guaranteed_held_on_path.back();

		// Now iterate overall critical sections, find per-CS resource
		// sets, and take the intersection.

		foreach(taskset, tx)
		{
			const unsigned int x = tx->get_id();

			// look at each critical section related to 'q'
			unsigned int cs_index;
			enumerate(taskset_cs[x].get_cs(), cs, cs_index)
			{
				if (cs->resource_id == q)
				{
					update_guaranteed_lock_set(x, cs_index, ghq);
				}
			}
		}

		neutral_element.insert(q);
	}
}

static void intersect(LockSet &a, const LockSet &b)
{
	LockSet tmp;

	std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
						  std::inserter(tmp, tmp.begin()));
	a.swap(tmp);
}

void NestedFifoILP::update_guaranteed_lock_set(
	unsigned int x, unsigned int cs_index,
	LockSet &guaranteed)
{
	if (taskset[x].get_cluster() == ti.get_cluster())
	{
		// intersect with the set of held locks
		intersect(guaranteed, outer_locks[x][cs_index]);
	}
	else if (taskset_cs[x].get_cs()[cs_index].is_nested())
	{
		// determine intersection of all guaranteed sets of all outer locks
		int parent = taskset_cs[x].get_cs()[cs_index].outer;
		const CriticalSection &pcs = taskset_cs[x].get_cs()[parent];
		LockSet per_cs = LockSet(guaranteed_held_on_path[pcs.resource_id]);

		parent = pcs.outer;
		// next, intersect with other outer locks, if any, by walking up the chain
		while (parent != CriticalSection::NO_PARENT)
		{
			const CriticalSection &ppcs = taskset_cs[x].get_cs()[parent];
			intersect(per_cs, guaranteed_held_on_path[ppcs.resource_id]);
			parent = ppcs.outer;
		}

		// finally, union with the set of locks held by (x, cs_index)
		per_cs.insert(outer_locks[x][cs_index].begin(),
		              outer_locks[x][cs_index].end());

		// intersect the per_cs set with the overall guarantee set
		intersect(guaranteed, per_cs);

		// also store this for check in add_remote_blocking_per_cs_constraints()
		guaranteed_held_cs_path[x][cs_index] = per_cs;
	}
	// else: nothing to do, "intersect" with the neutral element => no effect
}

// This implements Constraint 2 in the writeup.
void NestedFifoILP::add_arrival_blocking_constraints()
{
	LinearExpression *exp = new LinearExpression();

	/* for all local tasks of lower priority */
	foreach_local_lowereq_priority_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();

		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[tx->get_id()].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			const unsigned int q = cs->resource_id;

			/* for all instances of *cs while a job of Ti is pending */
			enumerate_cs_instances(*tx, tx_cs, cs_index, v)
			{
				var_t vertex_d = vars.vertex_direct(x, q, v);
				exp->add_var(vertex_d);
			}
		}
	}

	add_inequality(exp, 1);
}

// This matches Constraint 1 in the writeup.
void NestedFifoILP::add_local_resource_constraints()
{
	LinearExpression *exp = new LinearExpression();

	/* for all local, lower-priority tasks */
	foreach_local_lowereq_priority_task_except(taskset, ti, tx)
	{
		const unsigned int x = tx->get_id();
		/* for all vertices of tx */
		const CriticalSections& tx_cs = taskset_cs[x].get_cs();

		/* for all critical sections of Tx */
		unsigned int cs_index;
		enumerate(tx_cs, cs, cs_index)
		{
			const unsigned int q = cs->resource_id;
			if (is_local_resource_with_lower_prio_ceiling(q))
			{
				/* for all instances of *cs while a job of Ti is pending */
				enumerate_cs_instances(*tx, tx_cs, cs_index, v)
				{
					var_t vertex_d = vars.vertex_direct(x, q, v);
					exp->add_var(vertex_d);
				}
			}
		}
	}
	add_equality(exp, 0);
}

BlockingBounds* lp_nested_fifo_spinlock_bounds(
	const ResourceSharingInfo& info,
	const CriticalSectionsOfTaskset& tsk_cs)
{
	BlockingBounds* results = new BlockingBounds(info);

	for (unsigned int i=0; i<info.get_tasks().size(); i++)
	{
		NestedFifoILP ilp(info, tsk_cs, i);
		(*results)[i] = ilp.solve();
	}

	return results;
}
