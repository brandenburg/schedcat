#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"


BlockingBounds* global_fmlp_bounds(const ResourceSharingInfo& info)
{
	// split every thing by resources, sort, and then start counting.
	Resources resources;

	split_by_resource(info, resources);
	sort_by_request_length(resources);


	unsigned int i;
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	unsigned int num_tasks = info.get_tasks().size();

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference bterm;


		foreach(tsk.get_requests(), jt)
		{
			const RequestBound& req = *jt;
			const ContentionSet& cs =
				resources[req.get_resource_id()];

			unsigned long interval = tsk.get_response();
			unsigned long issued   = req.get_num_requests();

			// every other task may block once per request
			unsigned int total_limit = (num_tasks - 1) * issued;
			unsigned int per_src_limit = issued;

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

