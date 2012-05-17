#include "sharedres.h"
#include "blocking.h"
#include "rw-blocking.h"

#include "stl-helper.h"


static Interference pf_writer_fifo(
	const TaskInfo& tsk, const ClusterResources& writes,
	const unsigned int num_writes,
	const unsigned int num_reads,
	const unsigned int res_id,
	const unsigned int procs_per_cluster,
	const int dedicated_irq)
{
	const unsigned int per_src_wlimit = num_reads + num_writes;
	const unsigned long interval = tsk.get_response();
	ClusterLimits limits;
	int idx;

	limits.reserve(writes.size());
	enumerate(writes, ct, idx)
	{
		unsigned int total, parallelism = procs_per_cluster;

		if (idx == dedicated_irq)
			parallelism--;

		if (parallelism && (int) tsk.get_cluster() == idx)
			parallelism--;

		// At most one blocking request per remote CPU in
		// cluster per request.
		if (parallelism)
			total = num_reads + num_writes * parallelism;
		else
			// No interference from writers if we are hogging
			// the only available CPU.
			total = 0;

		limits.push_back(ClusterLimit(total, per_src_wlimit));
	}

	Interference blocking;
	blocking = bound_blocking_all_clusters(writes,
					       limits,
					       res_id,
					       interval,
					       &tsk);
	return blocking;

}

static Interference pf_reader_all(
	const TaskInfo& tsk,
	const Resources& all_reads,
	const unsigned int num_writes,
	const unsigned int num_wblock,
	const unsigned int num_reads,
	const unsigned int res_id,
	const unsigned int procs_per_cluster,
	const unsigned int num_procs)
{
	const unsigned long interval = tsk.get_response();
	Interference blocking;
	unsigned int rlimit = std::min(num_wblock + num_writes,
				   num_reads + num_writes * (num_procs - 1));
	blocking = bound_blocking(all_reads[res_id],
				  interval,
				  rlimit,
				  rlimit,
				  // exclude all if c == 1
				  procs_per_cluster == 1,
				  &tsk);
	return blocking;
}

BlockingBounds* clustered_rw_omlp_bounds(const ResourceSharingInfo& info,
					 unsigned int procs_per_cluster,
					 int dedicated_irq)
{
	// split everything by partition
	Clusters clusters;

	split_by_cluster(info, clusters);

	// split each partition by resource
	ClusterResources resources;

	split_by_resource(clusters, resources);

	// split all by resource
	Resources all_task_reqs, all_reads, __all_writes;
	split_by_resource(info, all_task_reqs);
	split_by_type(all_task_reqs, all_reads, __all_writes);

	// sort each contention set by request length
	sort_by_request_length(resources);
	sort_by_request_length(all_reads);

	// split by type --- sorted order is maintained
	ClusterResources __reads, writes;
	split_by_type(resources, __reads, writes);


	// We need for each task the maximum request span.  We also need the
	// maximum direct blocking from remote partitions for each request. We
	// can determine both in one pass.

	const unsigned int num_procs = procs_per_cluster * clusters.size();
	unsigned int i;

	// direct blocking results
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		RWCounts rwcounts;
		Interference bterm;

		merge_rw_requests(tsk, rwcounts);

		foreach(rwcounts, jt)
		{
			const RWCount& rw = *jt;

			// skip placeholders
			if (!rw.num_reads && !rw.num_writes)
				continue;

			Interference wblocking,  rblocking;

			wblocking = pf_writer_fifo(tsk, writes, rw.num_writes,
						   rw.num_reads, rw.res_id,
						   procs_per_cluster,
						   dedicated_irq);

			rblocking = pf_reader_all(tsk, all_reads, rw.num_writes,
						  wblocking.count, rw.num_reads,
						  rw.res_id, procs_per_cluster,
						  num_procs);

			//**** SINGLE WRITE
			Interference rblocking_w1, wblocking_w1;

			// Keep track of maximum request span.
			// Is this already a single-issue request?
			if (rw.num_writes &&
			    (rw.num_writes != 1 || rw.num_reads != 0))
			{
				wblocking_w1 = pf_writer_fifo(tsk, writes, 1, 0,
							      rw.res_id, procs_per_cluster,
							      dedicated_irq);

				rblocking_w1 = pf_reader_all(
					tsk, all_reads, 1,
					wblocking_w1.count, 0,
					rw.res_id, procs_per_cluster,
					num_procs);
			}
			else if (rw.num_writes)
			{
				  wblocking_w1 = wblocking;
				  rblocking_w1 = rblocking;
			}
			// else: zero, nothing to do

			//**** SINGLE READ

			Interference rblocking_r1, wblocking_r1;


			if (rw.num_reads &&
			    (rw.num_reads != 1 || rw.num_writes != 0))
			{
				wblocking_r1 = pf_writer_fifo(tsk, writes, 0, 1,
							      rw.res_id, procs_per_cluster,
							      dedicated_irq);

				rblocking_r1 = pf_reader_all(
					tsk, all_reads, 0,
					wblocking_r1.count, 1,
					rw.res_id, procs_per_cluster,
					num_procs);
			}
			else if (rw.num_reads)
			{
				wblocking_r1 = wblocking;
				rblocking_r1 = rblocking;
			}

			// else: zero, nothing to do

			// The span includes our own request.
			if (rw.num_writes)
			{
				wblocking_w1.total_length += rw.wlength;
				wblocking_w1.count        += 1;
			}
			if (rw.num_reads)
			{
				rblocking_r1.total_length += rw.rlength;
				wblocking_r1.count        += 1;
			}

			// combine
			wblocking_w1 += rblocking_w1;
			wblocking_r1 += rblocking_r1;
			wblocking    += rblocking;

			results.raise_request_span(i, wblocking_w1);
			results.raise_request_span(i, wblocking_r1);
			bterm += wblocking;
		}
		results[i] = bterm;
	}

	// This is the initial delay due to priority donation.
	charge_arrival_blocking(info, results);

	return _results;
}

BlockingBounds* phase_fair_rw_bounds(const ResourceSharingInfo& info,
				     unsigned int procs_per_cluster,
				     int dedicated_irq)
{
	// These are structurally equivalent. Therefore, no need to reimplement
	// everything from scratch.
	return clustered_rw_omlp_bounds(info, procs_per_cluster, dedicated_irq);
}
