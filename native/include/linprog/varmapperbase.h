#ifndef VARMAPPERBASE_H
#define VARMAPPERBASE_H

#include <stdint.h>

#include "stl-helper.h"
#include "stl-hashmap.h"

class VarMapperBase {

private:
	hashmap<uint64_t, unsigned int> map;
	unsigned int next_var;
	bool sealed;

protected:
	void insert(uint64_t key)
	{
		assert(next_var < UINT_MAX);
		assert(!sealed);

		unsigned int idx = next_var++;
		map[key] = idx;
	}

	bool exists(uint64_t key) const
	{
		return map.count(key) > 0;
	}

	unsigned int get(uint64_t key)
	{
		return map[key];
	}

	unsigned int var_for_key(uint64_t key)
	{
		if (!exists(key))
			insert(key);
		return get(key);
	}

	bool search_key_for_var(unsigned int var, uint64_t &key) const
	{
		foreach(map, it)
		{
			if (it->second == var)
			{
				key = it->first;
				return true;
			}
		}
		return false;
	}

public:

	VarMapperBase(unsigned int start_var = 0)
		: next_var(start_var), sealed(false)
	{}


	// stop new IDs from being generated
	void seal()
	{
		sealed = true;
	}

	unsigned int get_num_vars() const
	{
		return map.size();
	}

	unsigned int get_next_var() const
	{
		return next_var;
	}


	// debugging support

	std::string var2str(unsigned int var) const;

	// should be overridden by children
	virtual std::string key2str(uint64_t key, unsigned int var) const;

	hashmap<unsigned int, std::string> get_translation_table() const;
};


#endif
