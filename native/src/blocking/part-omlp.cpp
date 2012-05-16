#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"

BlockingBounds* part_omlp_bounds(const ResourceSharingInfo& info)
{
	// split everything by partition
	Clusters clusters;

	split_by_cluster(info, clusters);

	// split each partition by resource
	ClusterResources resources;

	split_by_resource(clusters, resources);

	// sort each contention set by request length
	sort_by_request_length(resources);

	// We need for each task the maximum request span.  We also need the
	// maximum direct blocking from remote partitions for each request. We
	// can determine both in one pass.

	unsigned int i;

	// direct blocking results
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference bterm;

		foreach(tsk.get_requests(), jt)
		{
			const RequestBound& req = *jt;

			Interference blocking;

			blocking = np_fifo_per_resource(
				tsk, resources, 1,
				req.get_resource_id(), req.get_num_requests());

			// add in blocking term
			bterm += blocking;

			// Keep track of maximum request span.
			// Is this already a single-issue request?
			if (req.get_num_requests() != 1)
				// nope, need to recompute
				blocking = np_fifo_per_resource(
					tsk, resources, 1,
					req.get_resource_id(), 1);

			// The span includes our own request.
			blocking.total_length += req.get_request_length();
			blocking.count        += 1;

			// Update max. request span.
			results.raise_request_span(i, blocking);
		}

		results[i] = bterm;
	}

	charge_arrival_blocking(info, results);

	return _results;
}
