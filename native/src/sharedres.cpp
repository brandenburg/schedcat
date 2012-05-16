#include <algorithm> // for greater, sort
#include <numeric>
#include <functional>
#include <limits.h>
#include <iostream>

#include "sharedres.h"
#include "res_io.h"

#include "time-types.h"
#include "math-helper.h"

#include "stl-helper.h"

#ifdef CONFIG_USE_0X
#include <unordered_map>
#define hashmap std::unordered_map
#else
#include <ext/hash_map>
#define hashmap __gnu_cxx::hash_map
#endif

#include "blocking.h"

const unsigned int UNLIMITED = UINT_MAX;

std::ostream& operator<<(std::ostream &os, const TaskInfo  &ti)
{
	os << "TaskInfo[";
	if (ti.get_priority() != UINT_MAX)
		os << "priority="
		   << ti.get_priority() <<  ", ";
	os << "period="
	   << ti.get_period()   << ", response="
	   << ti.get_response() << ", cluster="
	   << ti.get_cluster()  << ", requests=<";

	foreach(ti.get_requests(), it)
	{
		if (it != ti.get_requests().begin())
			os << " ";
		os << (*it);
	}

	os << ">]";
	return os;
}

std::ostream& operator<<(std::ostream &os, const RequestBound &rb)
{
	os << "(res-id="
	   << rb.get_resource_id() << ", num="
	   << rb.get_num_requests() << ", len="
	   << rb.get_request_length() << ")";
	return os;
}

std::ostream& operator<<(std::ostream &os, const ResourceSharingInfo &rsi)
{
	foreach(rsi.get_tasks(), it)
	{
		const TaskInfo& tsk  = *it;
		os << "\t" << tsk << std::endl;
	}
	return os;
}

unsigned int RequestBound::get_max_num_requests(unsigned long interval) const
{
	unsigned long num_jobs;

	num_jobs = divide_with_ceil(interval + task->get_response(),
				    task->get_period());

	return (unsigned int) (num_jobs * num_requests);
}


// ****** non-exported helpers *******


void split_by_cluster(const ResourceSharingInfo& info, Clusters& clusters)
{
	foreach(info.get_tasks(), it)
	{
		const TaskInfo& tsk  = *it;
		unsigned int cluster = tsk.get_cluster();

		while (cluster  >= clusters.size())
			clusters.push_back(Cluster());

		clusters[cluster].push_back(&tsk);
	}
}


bool has_higher_priority(const TaskInfo* a, const TaskInfo* b)
{
	return a->get_priority() < b->get_priority();
}

void sort_by_priority(Clusters& clusters)
{
	foreach(clusters, it)
	{
		Cluster& cluster = *it;
		std::sort(cluster.begin(), cluster.end(), has_higher_priority);
	}
}


void split_by_resource(const ResourceSharingInfo& info, Resources& resources)
{
	foreach(info.get_tasks(), it)
	{
		const TaskInfo& tsk  = *it;

		foreach(tsk.get_requests(), jt)
		{
			const RequestBound& req = *jt;
			unsigned int res = req.get_resource_id();

			while (res  >= resources.size())
				resources.push_back(ContentionSet());

			resources[res].push_back(&req);
		}
	}
}

void split_by_resource(const Cluster& cluster, Resources& resources)
{

	foreach(cluster, it)
	{
		const TaskInfo* tsk  = *it;

		foreach(tsk->get_requests(), jt)
		{
			const RequestBound& req = *jt;
			unsigned int res = req.get_resource_id();

			while (res  >= resources.size())
				resources.push_back(ContentionSet());

			resources[res].push_back(&req);
		}
	}
}

void split_by_resource(const Clusters& clusters,
		       ClusterResources& resources)
{
	foreach(clusters, it)
	{
		resources.push_back(Resources());
		split_by_resource(*it, resources.back());
	}
}

void split_by_type(const ContentionSet& requests,
			  ContentionSet& reads,
			  ContentionSet& writes)
{
	foreach(requests, it)
	{
		const RequestBound *req = *it;

		if (req->get_request_type() == READ)
			reads.push_back(req);
		else
			writes.push_back(req);
	}
}

void split_by_type(const Resources& resources,
		  Resources &reads,
		  Resources &writes)
{
	reads.reserve(resources.size());
	writes.reserve(resources.size());
	foreach(resources, it)
	{
		reads.push_back(ContentionSet());
		writes.push_back(ContentionSet());
		split_by_type(*it, reads.back(), writes.back());
	}
}

void split_by_type(const ClusterResources& per_cluster,
		  ClusterResources &reads,
		  ClusterResources &writes)
{
	reads.reserve(per_cluster.size());
	writes.reserve(per_cluster.size());
	foreach(per_cluster, it)
	{
		reads.push_back(Resources());
		writes.push_back(Resources());
		split_by_type(*it, reads.back(), writes.back());
	}
}

static bool has_longer_request_length(const RequestBound* a,
				      const RequestBound* b)
{
	return a->get_request_length() > b->get_request_length();
}

void sort_by_request_length(ContentionSet& cs)
{
	std::sort(cs.begin(), cs.end(), has_longer_request_length);
}

static bool has_longer_request_length_lcs(const LimitedRequestBound &a,
				          const LimitedRequestBound &b)
{
	return has_longer_request_length(a.request_bound, b.request_bound);
}

void sort_by_request_length(LimitedContentionSet &lcs)
{
	std::sort(lcs.begin(), lcs.end(), has_longer_request_length_lcs);
}

void sort_by_request_length(Resources& resources)
{
	apply_foreach(resources, sort_by_request_length);
}

void sort_by_request_length(ClusterResources& resources)
{
	apply_foreach(resources, sort_by_request_length);
}

typedef std::vector<TaskContention> ClusterContention;

typedef std::vector<ContentionSet> TaskContention;

Interference bound_blocking(const ContentionSet& cont,
			    unsigned long interval,
			    unsigned int max_total_requests,
			    unsigned int max_requests_per_source,
			    const TaskInfo* exclude_tsk,
			    // Note: the following parameter excludes
			    // *high-priority* tasks. Used to exclude local higher-priority tasks.
			    // Default: all tasks can block (suitable for remote blocking).
			    unsigned int min_priority /* default == 0 */)
{
	Interference inter;
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
			// request object per task. This makes sense if the
			// contention set has been split by resource. This may
			// be pessimistic for contention sets that contain
			// request objects for multiple resources. The
			// assumption also works out if max_total_requests ==
			// max_requests_per_source.
			num = std::min(req->get_max_num_requests(interval),
				       max_requests_per_source);
			num = std::min(num, remaining);

			inter.total_length += num * req->get_request_length();
			inter.count        += num;
			remaining -= num;
		}
	}

	return inter;
}

Interference bound_blocking(const ContentionSet& cont,
			    unsigned long interval,
			    unsigned int max_total_requests,
			    unsigned int max_requests_per_source,
			    bool exclude_whole_cluster,
			    const TaskInfo* exclude_tsk)
{
	Interference inter;
	unsigned int remaining;

	remaining = max_total_requests;

	foreach(cont, it)
	{
		const RequestBound* req = *it;

		if (!remaining)
			break;

		// only use this source if it is not excluded
		if (req->get_task() != exclude_tsk &&
		    (!exclude_whole_cluster ||
		     req->get_task()->get_cluster() != exclude_tsk->get_cluster()))
		{
			unsigned int num;
			num = std::min(req->get_max_num_requests(interval),
				       max_requests_per_source);
			num = std::min(num, remaining);

			inter.total_length += num * req->get_request_length();
			inter.count        += num;
			remaining -= num;
		}
	}

	return inter;
}

Interference bound_blocking_all_clusters(
	const ClusterResources& clusters,
	const ClusterLimits& limits,
	unsigned int res_id,
	unsigned long interval,
	const TaskInfo* exclude_tsk)
{
	Interference inter;
	unsigned int i;

	// add interference from each non-excluded cluster
	enumerate(clusters, it, i)
	{
		const Resources& resources = *it;
		const ClusterLimit& limit = limits[i];

		if (resources.size() > res_id)
			inter += bound_blocking(resources[res_id],
						interval,
						limit.max_total_requests,
						limit.max_requests_per_source,
						exclude_tsk);
	}

	return inter;
}

static Interference max_local_request_span(const TaskInfo &tsk,
					   const TaskInfos &tasks,
					   const BlockingBounds& bounds)
{
	Interference span;
	unsigned int i = 0;

	enumerate(tasks, it, i)
	{
		const TaskInfo& t = *it;

		if (&t != &tsk)
		{
			// only consider local, lower-priority tasks
			if (t.get_cluster() == tsk.get_cluster() &&
			    t.get_priority() >= tsk.get_priority())
			{
				Interference b = bounds.get_max_request_span(i);
				span = std::max(span, bounds.get_max_request_span(i));
			}
		}
	}

	return span;
}

void charge_arrival_blocking(const ResourceSharingInfo& info,
			     BlockingBounds& bounds)
{
	unsigned int i = 0;
	const TaskInfos& tasks = info.get_tasks();

	enumerate(tasks, it, i)
	{
		Interference inf = max_local_request_span(*it, tasks, bounds);
		bounds[i] += inf; // charge to total
		bounds.set_arrival_blocking(i, inf);
	}
}


// **** blocking term analysis ****

ClusterLimits np_fifo_limits(
	const TaskInfo& tsk, const ClusterResources& clusters,
	unsigned int procs_per_cluster,
	const unsigned int issued,
	int dedicated_irq)
{
	ClusterLimits limits;
	int idx;
	limits.reserve(clusters.size());
	enumerate(clusters, ct, idx)
	{
		unsigned int total, parallelism = procs_per_cluster;

		if (idx == dedicated_irq)
			parallelism--;

		if (parallelism && (int) tsk.get_cluster() == idx)
			parallelism--;

		// At most one blocking request per remote CPU in
		// cluster per request.
		total = issued * parallelism;
		limits.push_back(ClusterLimit(total, issued));
	}

	return limits;
}

Interference np_fifo_per_resource(
	const TaskInfo& tsk, const ClusterResources& clusters,
	unsigned int procs_per_cluster,
	unsigned int res_id, unsigned int issued,
	int dedicated_irq)
{
	const unsigned long interval = tsk.get_response();
	ClusterLimits limits = np_fifo_limits(tsk, clusters, procs_per_cluster,
					      issued, dedicated_irq);
	return bound_blocking_all_clusters(clusters,
					   limits,
					   res_id,
					   interval,
					  &tsk);
}

void merge_rw_requests(const TaskInfo &tsk, RWCounts &counts)
{
	foreach(tsk.get_requests(), req)
	{
		unsigned int res_id = req->get_resource_id();

		while (counts.size() <= res_id)
			counts.push_back(RWCount(counts.size()));

		if (req->is_read())
		{
			counts[res_id].num_reads += req->get_num_requests();
			counts[res_id].rlength = req->get_request_length();
		}
		else
		{
			counts[res_id].num_writes += req->get_num_requests();
			counts[res_id].wlength = req->get_request_length();
		}
	}
}




BlockingBounds* task_fair_mutex_bounds(const ResourceSharingInfo& info,
				       unsigned int procs_per_cluster,
				       int dedicated_irq)
{
	// These are structurally equivalent. Therefore, no need to reimplement
	// everything from scratch.
	return clustered_omlp_bounds(info, procs_per_cluster, dedicated_irq);
}


BlockingBounds* phase_fair_rw_bounds(const ResourceSharingInfo& info,
				     unsigned int procs_per_cluster,
				     int dedicated_irq)
{
	// These are structurally equivalent. Therefore, no need to reimplement
	// everything from scratch.
	return clustered_rw_omlp_bounds(info, procs_per_cluster, dedicated_irq);
}


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


