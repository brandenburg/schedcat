#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"

BlockingBounds* global_omlp_bounds(const ResourceSharingInfo& info,
				   unsigned int num_procs)
{
	// split every thing by resources, sort, and then start counting.
	Resources resources;

	split_by_resource(info, resources);
	sort_by_request_length(resources);

	unsigned int i;
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference bterm;

		foreach(tsk.get_requests(), jt)
		{
			const RequestBound& req = *jt;
			const ContentionSet& cs =
				resources[req.get_resource_id()];

			unsigned int num_sources = cs.size();
			unsigned long interval = tsk.get_response();
			unsigned long issued   = req.get_num_requests();


			unsigned int total_limit = (2 * num_procs - 1) * issued;
			// Derived in the dissertation: at most twice per request.
			unsigned int per_src_limit = 2 * issued;

			if (num_sources <= num_procs + 1) {
				// FIFO case: no job is ever skipped in the
				//  priority queue (since at most one job is in
				//  PQ at any time).
				// Lemma 15 in RTSS'10: at most one blocking
				// request per source per issued request.
				per_src_limit = issued;
				total_limit   = (num_sources - 1) * issued;
			}

			bterm += bound_blocking(cs,
						interval,
						total_limit,
						per_src_limit,
						&tsk);
		}

		results[i] = bterm;
	}

	return _results;
}

