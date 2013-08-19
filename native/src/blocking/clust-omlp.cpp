#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"
#include "math-helper.h"

BlockingBounds* clustered_omlp_bounds(const ResourceSharingInfo& info,
				      unsigned int procs_per_cluster,
				      int dedicated_irq)
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
				tsk, resources, procs_per_cluster,
				req.get_resource_id(),
				req.get_num_requests(),
				dedicated_irq);

			// add in blocking term
			bterm += blocking;

			// Keep track of maximum request span.
			// Is this already a single-issue request?
			if (req.get_num_requests() != 1)
				blocking = np_fifo_per_resource(
					tsk, resources, procs_per_cluster,
					req.get_resource_id(), 1);

			// The span includes our own request.
			blocking.total_length += req.get_request_length();
			blocking.count        += 1;
			// Update max. request span.
			results.raise_request_span(i, blocking);
		}

		results[i] = bterm;
		results.set_remote_blocking(i, bterm);
	}

	// This is the initial delay due to priority donation.
	charge_arrival_blocking(info, results);

	return _results;
}

BlockingBounds* task_fair_mutex_bounds(const ResourceSharingInfo& info,
				       unsigned int procs_per_cluster,
				       int dedicated_irq)
{
	// These are structurally equivalent. Therefore, no need to reimplement
	// everything from scratch.
	return clustered_omlp_bounds(info, procs_per_cluster, dedicated_irq);
}

static void add_blocking(LimitedContentionSet &lcs,
			 const ContentionSet& cont,
			 unsigned long interval,
			 unsigned int max_total_requests,
			 unsigned int max_requests_per_source,
			 const TaskInfo* exclude_tsk,
			 unsigned int min_priority = 0)
{
	unsigned int remaining;

	remaining = max_total_requests;

	foreach(cont, it)
	{
		const RequestBound* req = *it;

		if (!remaining)
			break;

		// only use this source if it is not excluded
		if (req->get_task() != exclude_tsk &&
		    req->get_task()->get_priority() >= min_priority)
		{
			unsigned int num;
			// This makes the assumption that there is only one
			// request object per task. See bound_blocking() above.
			num = std::min(req->get_max_num_requests(interval),
				       max_requests_per_source);
			num = std::min(num, remaining);
			remaining -= num;
			lcs.push_back(LimitedRequestBound(req, num));
		}
	}
}

// Return a contention set that includes the longest requests from all
// clusters subject to the specified constraints.
static LimitedContentionSet contention_from_all_clusters(
	const ClusterResources& clusters,
	const ClusterLimits& limits,
	unsigned int res_id,
	unsigned long interval,
	const TaskInfo* exclude_tsk)
{
	LimitedContentionSet lcs;
	unsigned int i;

	// add interference from each non-excluded cluster
	enumerate(clusters, it, i)
	{
		const Resources& resources = *it;
		const ClusterLimit& limit = limits[i];

		if (resources.size() > res_id)
			add_blocking(lcs, resources[res_id],
				     interval,
				     limit.max_total_requests,
				     limit.max_requests_per_source,
				     exclude_tsk);
	}

	return lcs;
}

static LimitedContentionSet np_fifo_per_resource_contention(
	const TaskInfo& tsk, const ClusterResources& clusters,
	unsigned int procs_per_cluster,
	unsigned int res_id, unsigned int issued,
	int dedicated_irq = NO_CPU)
{
	const unsigned long interval = tsk.get_response();
	ClusterLimits limits = np_fifo_limits(tsk, clusters, procs_per_cluster,
					      issued, dedicated_irq);
	return contention_from_all_clusters(clusters,
					    limits,
					    res_id,
					    interval,
					   &tsk);
}

// assumption: lcs is sorted by request length
static Interference bound_blocking(const LimitedContentionSet &lcs, unsigned int max_total_requests)
{
	Interference inter;
	unsigned int remaining = max_total_requests;

	foreach(lcs, it)
	{
		const LimitedRequestBound &lreqb = *it;
		unsigned int num;

		if (!remaining)
			break;

		num = std::min(lreqb.limit, remaining);

		inter.total_length += num * lreqb.request_bound->get_request_length();
		inter.count        += num;
		remaining          -= num;
	}

	return inter;
}


BlockingBounds* clustered_kx_omlp_bounds(const ResourceSharingInfo& info,
					 const ReplicaInfo& replicaInfo,
					 unsigned int procs_per_cluster,
					 int dedicated_irq)
{
	// split everything by partition
	Clusters clusters;

	split_by_cluster(info, clusters);

	const unsigned int num_cpus = clusters.size() * procs_per_cluster -
	                              (dedicated_irq != NO_CPU ? 1 : 0);

	// split each partition by resource
	ClusterResources resources;

	split_by_resource(clusters, resources);

	// sort each contention set by request length
	sort_by_request_length(resources);

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

			unsigned int max_total_once;
			LimitedContentionSet lcs;
			Interference blocking;

			max_total_once = divide_with_ceil(num_cpus,
					replicaInfo[req.get_resource_id()]) - 1;

			lcs = np_fifo_per_resource_contention(
					tsk, resources, procs_per_cluster,
					req.get_resource_id(),
					req.get_num_requests(),
					dedicated_irq);
			sort_by_request_length(lcs);
			blocking = bound_blocking(lcs, max_total_once * req.get_num_requests());

			// add in blocking term
			bterm += blocking;

			// Keep track of maximum request span.
			// Is this already a single-issue request?
			if (req.get_num_requests() != 1)
			{
				lcs = np_fifo_per_resource_contention(
						tsk, resources, procs_per_cluster,
						req.get_resource_id(),
						1, dedicated_irq);
				sort_by_request_length(lcs);
				blocking = bound_blocking(lcs, max_total_once);
			}

			// The span includes our own request.
			blocking.total_length += req.get_request_length();
			blocking.count        += 1;
			// Update max. request span.
			results.raise_request_span(i, blocking);
		}

		results[i] = bterm;
	}

	// This is the initial delay due to priority donation.
	charge_arrival_blocking(info, results);

	return _results;
}
