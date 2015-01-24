#ifndef STL_HELPER_H
#define STL_HELPER_H

#include <algorithm>

#define foreach(collection, it)						\
	for (auto it = (collection).begin();	\
	     it != (collection).end();					\
		     it++)

#define enumerate(collection, it, i)					\
	for (auto it = ({i = 0; (collection).begin();}); \
	     it != (collection).end();					\
	     it++, i++)

#define apply_foreach(collection, fun, ...)				\
	foreach(collection, __apply_it_ ## collection) {		\
		fun(*__apply_it_ ## collection, ## __VA_ARGS__);	\
	}

#define map_ref(from, to, init, fun, ...)					\
	{								\
		(to).clear();						\
		(to).reserve((from).size());				\
		foreach(from, __map_ref_it) {				\
			(to).push_back(init());				\
			fun(*__map_ref_it, (to).back(),			\
			    ## __VA_ARGS__);				\
		}							\
	}

// From: http://stackoverflow.com/questions/1964150/c-test-if-2-sets-are-disjoint
template<class Set1, class Set2>
bool is_disjoint(const Set1 &set1, const Set2 &set2)
{
    if(set1.empty() || set2.empty()) return true;

    typename Set1::const_iterator
        it1 = set1.begin(),
        it1End = set1.end();
    typename Set2::const_iterator
        it2 = set2.begin(),
        it2End = set2.end();

    if(*it1 > *set2.rbegin() || *it2 > *set1.rbegin()) return true;

    while(it1 != it1End && it2 != it2End)
    {
        if(*it1 == *it2) return false;
        if(*it1 < *it2) { it1++; }
        else { it2++; }
    }

    return true;
}

template<class Set1, class Set2>
bool is_subset_of(const Set1 &set1, const Set2 &set2)
{
	return std::includes(set2.begin(), set2.end(),
	                     set1.begin(), set1.end());
}


#endif
