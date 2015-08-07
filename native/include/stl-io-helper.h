#ifndef STL_IO_HELPER_H
#define STL_IO_HELPER_H

#include <iostream>
#include <set>
#include <vector>
#include <map>

#include "stl-hashmap.h"
#include "stl-helper.h"

template <typename T>
std::ostream& operator<<(std::ostream &os, const std::set<T> &s)
{
	bool first = true;
	os << "{";
	foreach(s, e)
	{
		if (!first)
			os << ", ";
		os << *e;
		first = false;
	}
	os << "}";

	return os;
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream &os, const std::map<K,V> &s)
{
	bool first_elem = true;
	os << "{";
	foreach(s, e)
	{
		if (!first_elem)
			os << ", ";
		os << e->first;
		os << ": ";
		os << e->second;
		first_elem = false;
	}
	os << "}";

	return os;
}

template <typename T>
std::ostream& operator<<(std::ostream &os, const hashset<T> &s)
{
	bool first = true;
	os << "{";
	foreach(s, e)
	{
		if (!first)
			os << ", ";
		os << *e;
		first = false;
	}
	os << "}";

	return os;
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream &os, const hashmap<K,V> &s)
{
	bool first_elem = true;
	os << "{";
	foreach(s, e)
	{
		if (!first_elem)
			os << ", ";
		os << e->first;
		os << ": ";
		os << e->second;
		first_elem = false;
	}
	os << "}";

	return os;
}

template <typename T>
std::ostream& operator<<(std::ostream &os, const std::vector<T> &s)
{
	bool first = true;
	os << "[";
	foreach(s, e)
	{
		if (!first)
			os << ", ";
		os << *e;
		first = false;
	}
	os << "]";

	return os;
}


#define NYI() \
	{ \
		std::cerr << std::endl \
			<< __FUNCTION__ << " in " << __FILE__ \
			<< ": NOT YET IMPLEMENTED!" << std::endl; \
			abort(); \
	}

#endif
