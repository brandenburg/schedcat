#ifndef NESTED_CS_H
#define NESTED_CS_H

#ifndef SWIG
#include <vector>
#include <assert.h>
#include <iostream>
#include <set>
#include "stl-hashmap.h"
#endif

class CriticalSectionsOfTask;

typedef std::set<unsigned int> LockSet;

struct CriticalSection
{
	unsigned int resource_id;
	unsigned int length; /* excluding nested requests, if any */
	int outer; /* index of containing critical section, -1 if outermost */

	enum {
		NO_PARENT = -1,
	};

	CriticalSection(unsigned int res_id, unsigned int len,
	                int outer_cs = NO_PARENT)
		: resource_id(res_id), length(len), outer(outer_cs) {}

	// return the set of resources already held when this resource is requested
	LockSet get_outer_locks(const CriticalSectionsOfTask &task) const;

	bool is_nested() const
	{
		return outer != NO_PARENT;
	}

	bool is_outermost() const
	{
		return outer == NO_PARENT;
	}

	bool has_common_outer(
		const CriticalSectionsOfTask &this_task,
		const LockSet &already_held_by_other) const;

	bool has_common_outer(
		const CriticalSectionsOfTask &this_task,
		const CriticalSection &other_cs,
		const CriticalSectionsOfTask &other_task) const
	{
		/* first check that neither is outermost */
		if (is_outermost() || other_cs.is_outermost())
			return false;
		else
			return other_cs.has_common_outer(
				this_task, other_cs.get_outer_locks(other_task));
	}
};


typedef std::vector<CriticalSection> CriticalSections;

class CriticalSectionsOfTask
{
	CriticalSections cs;

public:

	const CriticalSections& get_cs() const
	{
		return cs;
	}

	operator const CriticalSections&() const
	{
		return cs;
	}

	void add(unsigned int res_id, unsigned int len,
	         int outer_cs = CriticalSection::NO_PARENT)
	{
		assert( outer_cs == CriticalSection::NO_PARENT
		        || (unsigned long) outer_cs < cs.size() );
		cs.push_back(CriticalSection(res_id, len, outer_cs));
	}

	bool has_nested_requests(unsigned int cs_index) const
	{
		for (int i = cs_index + 1; i < (int) cs.size(); i++)
			if (cs[i].outer == (int) cs_index)
				return true;
		return false;
	}

	unsigned int get_outermost(unsigned int cs_index) const
	{
		unsigned int cur = cs_index;

		while (cs[cur].is_nested())
			cur = cs[cur].outer;

		return cur;
	}

};



typedef std::vector<CriticalSectionsOfTask> CriticalSectionsOfTasks;

class CriticalSectionsOfTaskset
{
	CriticalSectionsOfTasks tsks;

public:

	const CriticalSectionsOfTasks& get_tasks() const
	{
		return tsks;
	}

	operator const CriticalSectionsOfTasks&() const
	{
		return tsks;
	}

	CriticalSectionsOfTask& new_task()
	{
		tsks.push_back(CriticalSectionsOfTask());
		return tsks.back();
	}

	/* Compute for each resource 'q' the set of resources that could be
	 * transitively requested while holding 'q'. */
	hashmap<unsigned int, hashset<unsigned int> >
	get_transitive_nesting_relationship() const;
};


void dump(const CriticalSectionsOfTaskset &x);

BlockingBounds* lp_nested_fifo_spinlock_bounds(
	const ResourceSharingInfo& info,
	const CriticalSectionsOfTaskset& tsk_cs);

#endif
