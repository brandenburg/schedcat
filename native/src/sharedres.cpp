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
#include "stl-hashmap.h"

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

unsigned int TaskInfo::get_max_num_jobs(unsigned long interval) const
{
	unsigned long num_jobs;
	num_jobs = divide_with_ceil(interval + get_response(), get_period());
	return num_jobs;
}

unsigned int TaskInfo::uni_fp_local_get_max_num_jobs(unsigned long interval) const
{
	unsigned long num_jobs;
	num_jobs = divide_with_ceil(interval, get_period());
	return num_jobs;
}

unsigned int RequestBound::get_max_num_requests(unsigned long interval) const
{
	return (unsigned int) (task->get_max_num_jobs(interval) * num_requests);
}


// ****** non-exported helpers *******


void split_by_cluster(const ResourceSharingInfo& info, Clusters& clusters, unsigned int num_cpus)
{
	if (num_cpus > 0)
		while (num_cpus > clusters.size())
			clusters.push_back(Cluster());
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

void determine_priority_ceilings(const Resources& resources,
					PriorityCeilings& ceilings)
{
	ceilings.reserve(resources.size());

	foreach(resources, it)
	{
		unsigned int ceiling = UINT_MAX;
		const ContentionSet& cs = *it;

		foreach(cs, jt)
		{
			const RequestBound* req = *jt;
			ceiling = std::min(ceiling, req->get_task()->get_priority());
		}

		ceilings.push_back(ceiling);
	}
}

PriorityCeilings get_priority_ceilings(const ResourceSharingInfo& info)
{
	Resources resources;
	PriorityCeilings ceilings;

	split_by_resource(info, resources);
	determine_priority_ceilings(resources, ceilings);

	return ceilings;
}

ResourceSet get_local_resources(const ResourceSharingInfo& info)
{
	ResourceSet locals;
	hashmap<unsigned int, unsigned int> accessed_in;

	foreach(info.get_tasks(), tsk)
	{
		foreach(tsk->get_requests(), req)
		{
			unsigned int res = req->get_resource_id();
			if (accessed_in.find(res) == accessed_in.end())
			{
				// resource not yet encountered
				accessed_in[res] = tsk->get_cluster();
				// assume res is local until proven otherwise
				locals.insert(res);
			}
			else if (accessed_in[res] != tsk->get_cluster())
			{
				// resource previously encountered and
				// accessed from at least two different clusters
				// => not local, remove res from set of locals
				locals.erase(res);
			}
		}
	}

	return locals;
}

static ResourceSharingInfo extract_resources(
	const ResourceSharingInfo& info,
	const ResourceSet& locals,
	const bool want_local)
{
	ResourceSharingInfo rsi(info.get_tasks().size());

	foreach(info.get_tasks(), tsk)
	{
		// copy task
		rsi.add_task(
			tsk->get_period(),
		        tsk->get_response(),
		        tsk->get_cluster(),
		        tsk->get_priority());

		foreach(tsk->get_requests(), req)
		{
			unsigned int res = req->get_resource_id();
			if ((locals.find(res) != locals.end() && want_local) ||
			    (locals.find(res) == locals.end() && !want_local))
			{
				rsi.add_request(
					res,
					req->get_num_requests(),
					req->get_request_length());
			}
		}
	}

	return rsi;
}

ResourceSharingInfo extract_local_resources(
	const ResourceSharingInfo& info,
	const ResourceSet& locals)
{
	return extract_resources(info, locals, true);
}

ResourceSharingInfo extract_global_resources(
	const ResourceSharingInfo& info,
	const ResourceSet& locals)
{
	return extract_resources(info, locals, false);
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
				span = std::max(span, b);
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

#include "rw-blocking.h"

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
