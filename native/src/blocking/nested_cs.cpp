#include "stl-hashmap.h"
#include "stl-helper.h"

#include "sharedres_types.h"
#include "nested_cs.h"


static void build_trans_nest_rel(
	hashmap<unsigned int, hashset<unsigned int> > &directly_nested,
	hashmap<unsigned int, hashset<unsigned int> > &trans_nested,
	unsigned int res)
{
	if (trans_nested.find(res) == trans_nested.end())
	{
		// assumes cycle-freedom

		// create set for res
		trans_nested[res] = hashset<unsigned int>();

		// populate by merging sets of children
		// 1) compute rel. for nested resources
		hashset<unsigned int> &s = trans_nested[res];
		foreach(directly_nested[res], nres)
		{
			build_trans_nest_rel(directly_nested, trans_nested, *nres);
			s.insert(*nres);
			s.insert(trans_nested[*nres].begin(), trans_nested[*nres].end());
		}
	}
	// Otherwise already computed, nothing to do.
}

/* Compute for each resource 'q' the set of resources that could be
	 * transitively requested while holding 'q'. */
hashmap<unsigned int, hashset<unsigned int> >
CriticalSectionsOfTaskset::get_transitive_nesting_relationship() const
{
	hashmap<unsigned int, hashset<unsigned int> > directly_nested;

	foreach(tsks, t)
	{
		foreach(t->get_cs(), cs)
		{
			if (directly_nested.find(cs->resource_id) == directly_nested.end())
				directly_nested[cs->resource_id] = hashset<unsigned int>();

			int outer = cs->outer;
			unsigned int nested_res = cs->resource_id;

			if (outer != CriticalSection::NO_PARENT)
			{
				unsigned int parent = t->get_cs()[outer].resource_id;
				directly_nested[parent].insert(nested_res);
			}
		}
	}

	hashmap<unsigned int, hashset<unsigned int> > nested;
	foreach(directly_nested, res)
		build_trans_nest_rel(directly_nested, nested, res->first);

	return nested;
}


LockSet CriticalSection::get_outer_locks(const CriticalSectionsOfTask &task) const
{
	LockSet already_held;

	int held = outer;
	while (held != NO_PARENT)
	{
		unsigned int parent = task.get_cs()[held].resource_id;
		already_held.insert(parent);
		held = task.get_cs()[held].outer;
	}

	return already_held;
}

bool CriticalSection::has_common_outer(
	const CriticalSectionsOfTask &this_task,
	const LockSet &already_held_by_other) const
{
	int held = outer;
	while (held != NO_PARENT)
	{
		unsigned int parent = this_task.get_cs()[held].resource_id;
		if (already_held_by_other.find(parent) != already_held_by_other.end())
			return true;
		held = this_task.get_cs()[held].outer;
	}

	return false;
}
