#ifndef STL_HELPER_H
#define STL_HELPER_H

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

#endif
