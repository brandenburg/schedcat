#include "sharedres.h"
#include "blocking.h"
#include "rw-blocking.h"

#include "stl-helper.h"
#include "stl-hashmap.h"

static Interference bound_blocking_all(
	const TaskInfo* tsk,
	const ContentionSet& all_reqs, // presumed sorted, for all clusters/tasks
	const unsigned int max_remote_requests, // per cluster
	const unsigned int max_local_requests,  // local cluster
	const unsigned int max_requests,        // per task
	unsigned int max_total)                 // stop after counting max_total
{
	unsigned long interval = tsk->get_response();
	hashmap<unsigned long, unsigned int> task_counter(512);
	hashmap<unsigned long, unsigned int>::iterator tctr;
	hashmap<unsigned int, unsigned int> cluster_counter(64);
	hashmap<unsigned int, unsigned int>::iterator cctr;
	Interference inter;

	cluster_counter[tsk->get_cluster()] = max_local_requests;

	foreach(all_reqs, it)
	{
		const RequestBound* req = *it;
		const TaskInfo* t = req->get_task();
		unsigned long key = (unsigned long) t;
		unsigned int cluster = t->get_cluster();

		if (!max_total)
			// we are done
			break;

		if (t == tsk)
			// doesn't block itself
			continue;

		// make sure we have seen this task
		tctr = task_counter.find(key);
		if (tctr == task_counter.end())
		{
			task_counter[key] = max_requests;
			tctr = task_counter.find(key);
		}

		if (!tctr->second)
			continue;

		cctr = cluster_counter.find(cluster);
		if (cctr == cluster_counter.end())
		{
			cluster_counter[cluster] = max_remote_requests;
			cctr = cluster_counter.find(cluster);
		}

		if (!cctr->second)
			continue;

		unsigned int remaining;
		remaining = std::min(tctr->second, cctr->second);
		remaining = std::min(remaining, max_total);
		unsigned int num = std::min(req->get_max_num_requests(interval), remaining);

		inter.total_length += num * req->get_request_length();
		inter.count        += num;
		cctr->second -= num;
		tctr->second -= num;
		max_total    -= num;
	}

	return inter;
}

static Interference tf_reader_all(
	const TaskInfo& tsk,
	const Resources& all_reads,
	const unsigned int num_writes,
	const unsigned int num_wblock,
	const unsigned int num_reads,
	const unsigned int res_id,
	const unsigned int procs_per_cluster)
{
	Interference blocking;
	unsigned int num_reqs = num_reads + num_writes;
	unsigned int max_reader_phases = num_wblock + num_writes;
	unsigned int task_limit = std::min(max_reader_phases, num_reqs);

	return bound_blocking_all(
		&tsk, all_reads[res_id],
		num_reqs * procs_per_cluster,
		num_reqs * (procs_per_cluster - 1),
		task_limit,
		max_reader_phases);
}


BlockingBounds* task_fair_rw_bounds(const ResourceSharingInfo& info,
				    const ResourceSharingInfo& info_mtx,
				    unsigned int procs_per_cluster,
				    int dedicated_irq)
{
	// split everything by partition
	Clusters clusters, clusters_mtx;

	split_by_cluster(info, clusters);
	split_by_cluster(info_mtx, clusters_mtx);

	// split each partition by resource
	ClusterResources resources, resources_mtx;

	split_by_resource(clusters, resources);
	split_by_resource(clusters_mtx, resources_mtx);

	// split all by resource
	Resources all_task_reqs, all_reads, __all_writes;
	split_by_resource(info, all_task_reqs);
	split_by_type(all_task_reqs, all_reads, __all_writes);

	// sort each contention set by request length
	sort_by_request_length(resources);
	sort_by_request_length(resources_mtx);
	sort_by_request_length(all_reads);

	// split by type --- sorted order is maintained
	ClusterResources __reads, writes;
	split_by_type(resources, __reads, writes);


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
		RWCounts rwcounts;

		Interference bterm;

		merge_rw_requests(tsk, rwcounts);

		foreach(rwcounts, jt)
		{
			const RWCount& rw = *jt;

			// skip placeholders
			if (!rw.num_reads && !rw.num_writes)
				continue;


			// 1) treat it as a mutex as a baseline
			Interference mtx, mtx_1;

			mtx = np_fifo_per_resource(
				tsk, resources_mtx, procs_per_cluster, rw.res_id,
				rw.num_reads + rw.num_writes,
				dedicated_irq);

			if (rw.num_reads + rw.num_writes == 1)
				mtx_1 = mtx;
			else
				mtx_1 = np_fifo_per_resource(
					tsk, resources_mtx, procs_per_cluster,
					rw.res_id, 1, dedicated_irq);

			// The span includes our own request.
			mtx_1.total_length += std::max(rw.wlength, rw.rlength);
			mtx_1.count        += 1;

			// 2) apply real RW analysis
			Interference wblocking, wblocking_1;
			Interference rblocking, rblocking_r1, rblocking_w1;

			wblocking = np_fifo_per_resource(
				tsk, writes, procs_per_cluster, rw.res_id,
				rw.num_reads + rw.num_writes,
				dedicated_irq);
			wblocking_1 = np_fifo_per_resource(
				tsk, writes, procs_per_cluster, rw.res_id, 1,
				dedicated_irq);

			rblocking = tf_reader_all(
				tsk, all_reads, rw.num_writes, wblocking.count,
				rw.num_reads, rw.res_id, procs_per_cluster);

			if (rw.num_writes)
			{
				// single write
				rblocking_w1 = tf_reader_all(
					tsk, all_reads, 1, wblocking.count,
					0, rw.res_id, procs_per_cluster);
				// The span includes our own request.
				rblocking_w1.total_length += rw.wlength;
				rblocking_w1.count        += 1;
			}
			if (rw.num_reads)
			{
				// single read
				rblocking_r1 = tf_reader_all(
					tsk, all_reads, 0, wblocking.count,
					1, rw.res_id, procs_per_cluster);
				// The span includes our own request.
				rblocking_r1.total_length += rw.rlength;
				rblocking_r1.count        += 1;
			}

			// combine
			wblocking   += rblocking;
			wblocking_1 += std::max(rblocking_w1, rblocking_r1);

			bterm += std::min(wblocking, mtx);
			results.raise_request_span(i, std::min(wblocking_1, mtx_1));
		}
		results[i] = bterm;
	}

	// This is the initial delay due to priority donation.
	charge_arrival_blocking(info, results);

	return _results;
}
