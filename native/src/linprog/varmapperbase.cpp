#include <cassert>
#include <climits>
#include <stdint.h>

#include <sstream>

#include "linprog/varmapperbase.h"


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
