#include <numeric>

#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"

typedef std::vector<ContentionSet> TaskContention;

static void all_from_cluster(const Cluster& cluster, ContentionSet& cs)
{
	foreach(cluster, it)
	{
		const TaskInfo* tsk  = *it;

		foreach(tsk->get_requests(), jt)
		{
			const RequestBound& req = *jt;
			cs.push_back(&req);
		}
	}
}

static void all_per_cluster(const Clusters& clusters,
			    AllPerCluster& all)
{
	foreach(clusters, it)
	{
		all.push_back(ContentionSet());
		all_from_cluster(*it, all.back());
	}
}

// have one contention set per task
static void derive_task_contention(const Cluster& cluster,
				   TaskContention& requests)
{
	requests.reserve(cluster.size());

	foreach(cluster, it)
	{
		const TaskInfo* tsk  = *it;

		requests.push_back(ContentionSet());

		foreach(tsk->get_requests(), jt)
		{
			const RequestBound& req = *jt;

			requests.back().push_back(&req);
		}
	}
}

static void derive_task_contention(const Clusters& clusters,
				   ClusterContention& contention)
{
	map_ref(clusters, contention, TaskContention, derive_task_contention);
}

/* this analysis corresponds to the FMLP+ in the dissertation */

static void pfmlp_count_direct_blocking(const TaskInfo* tsk,
					const ClusterResources& resources,
					std::vector<Interference>& counts)
{
	unsigned int interval = tsk->get_response();


	// for each resource requested by tsk
	foreach(tsk->get_requests(), jt)
	{
		const RequestBound& req = *jt;
		unsigned long issued    = req.get_num_requests();
		unsigned int res_id     = req.get_resource_id();

		unsigned int i;

		// for each cluster
		for (i = 0; i < resources.size(); i++)
		{
			// count interference... direct blocking will be counted later
			// make sure that cluster acceses res_id at  all
			if (resources[i].size() > res_id)
				// yes it does---how often can it block?
				counts[i] += bound_blocking(resources[i][res_id],
							    interval,
							    UNLIMITED,  // no total limit
							    issued, // once per request
							    tsk);
		}
	}
}

typedef std::vector<unsigned int> AccessCounts;
typedef std::vector<AccessCounts> PerClusterAccessCounts;

// How many times does a task issue requests that can
// conflict with tasks in a remote cluster. Indexed by cluster id.
typedef std::vector<unsigned int> IssuedRequests;
// Issued requests for each task. Indexed by task id.
typedef std::vector<IssuedRequests> PerTaskIssuedCounts;

static void derive_access_counts(const ContentionSet &cluster_contention,
				 AccessCounts &counts)
{
	foreach(cluster_contention, it)
	{
		const RequestBound *req = *it;
		unsigned int res_id = req->get_resource_id();

		while (counts.size() <= res_id)
			counts.push_back(0);

		counts[res_id] += req->get_num_requests();
	}
}

static void count_accesses_for_task(const TaskInfo& tsk,
				    const PerClusterAccessCounts& acc_counts,
				    IssuedRequests& ireqs)
{
	foreach(acc_counts, it)
	{
		const AccessCounts &ac = *it;
		unsigned int count = 0;

		// Check for each request of the task to see
		// if it conflicts with the cluster.
		foreach(tsk.get_requests(), jt)
		{
			const RequestBound &req = *jt;
			unsigned int res_id = req.get_resource_id();
			if (ac.size() > res_id && ac[res_id] > 0)
			{
				// cluster acceses res_id as well
				count += req.get_num_requests();
			}
		}
		ireqs.push_back(count);
	}
}

static void derive_access_counts(const AllPerCluster &per_cluster,
				 const ResourceSharingInfo &info,
				 PerTaskIssuedCounts &issued_reqs)
{
	PerClusterAccessCounts counts;

	/* which resources are accessed by each cluster? */
	map_ref(per_cluster, counts, AccessCounts, derive_access_counts);

	issued_reqs.reserve(info.get_tasks().size());

	foreach(info.get_tasks(), it)
	{
		issued_reqs.push_back(IssuedRequests());
		count_accesses_for_task(*it, counts, issued_reqs.back());
	}
}

static Interference pfmlp_bound_remote_blocking(const TaskInfo* tsk,
						const IssuedRequests &icounts,
						const std::vector<Interference>& counts,
						const ClusterContention& contention)
{
	unsigned int i;

	unsigned long interval = tsk->get_response();
	Interference blocking;

	// for each cluster
	for (i = 0; i < contention.size(); i++)
	{
		// Each task can either directly or indirectly block tsk
		// each time that tsk is directly blocked, but no more than
		// once per request issued by tsk.
		unsigned int max_per_task = std::min(counts[i].count, icounts[i]);

		// skip local cluster and independent clusters
		if (i == tsk->get_cluster() || !max_per_task)
			continue;

		Interference b;

		// for each task in cluster
		foreach(contention[i], it)
		{

			// count longest critical sections
			b += bound_blocking(*it,
					    interval,
					    max_per_task,
					    UNLIMITED, // no limit per source
					    tsk);
		}

		blocking += b;
	}
	return blocking;
}

static Interference pfmlp_bound_np_blocking(const TaskInfo* tsk,
					    const std::vector<Interference>& counts,
					    const AllPerCluster& per_cluster)
{
	unsigned int i;

	unsigned long interval = tsk->get_response();
	Interference blocking;

	// for each cluster
	for (i = 0; i < per_cluster.size(); i++)
	{
		// skip local cluster, this is only remote
		if (i == tsk->get_cluster())
			continue;

		// could be the same task each time tsk is directly blocked
		unsigned int max_direct = counts[i].count;
		Interference b;

		// count longest critical sections
		b += bound_blocking(per_cluster[i],
				    interval,
				    max_direct,
				    max_direct,
				    tsk);
		blocking += b;
	}
	return blocking;
}

static Interference pfmlp_bound_local_blocking(const TaskInfo* tsk,
					       const std::vector<Interference>& counts,
					       const ClusterContention& contention)
{
	// Locally, we have to account two things.
	// 1) Direct blocking from lower-priority tasks.
	// 2) Boost blocking from lower-priority tasks.
	// (Higher-priority requests are not counted as blocking.)
	// Since lower-priority jobs are boosted while
	// they directly block, 1) is subsumed by 2).
	// Lower-priority tasks cannot issue requests while a higher-priority
	// job executes. Therefore, at most one blocking request
	// is issued prior to the release of the job under analysis,
	// and one prior to each time that the job under analysis resumes.

	Interference blocking;
	Interference num_db = std::accumulate(counts.begin(), counts.end(),
					      Interference());
	unsigned int num_arrivals = std::min(tsk->get_num_arrivals(),
					     num_db.count + 1);
	unsigned long interval = tsk->get_response();

	const TaskContention& cont = contention[tsk->get_cluster()];

	// for each task in cluster
	foreach(cont, it)
	{
		// count longest critical sections
		blocking += bound_blocking(*it,
					   interval,
					   num_arrivals,
					   UNLIMITED, // no limit per source
					   tsk,
					   tsk->get_priority());
	}

	return blocking;
}

BlockingBounds* part_fmlp_bounds(const ResourceSharingInfo& info, bool preemptive)
{
	// split everything by partition
	Clusters clusters;

	split_by_cluster(info, clusters);

	// split each partition by resource
	ClusterResources resources;
	split_by_resource(clusters, resources);

	// find interference on a per-task basis
	ClusterContention contention;
	derive_task_contention(clusters, contention);

	// sort each contention set by request length
	sort_by_request_length(contention);

	// find total interference on a per-cluster basis
	AllPerCluster per_cluster;
	PerTaskIssuedCounts access_counts;

	all_per_cluster(clusters, per_cluster);
	sort_by_request_length(per_cluster);

	derive_access_counts(per_cluster, info, access_counts);

	// We need to find two blocking sources. Direct blocking (i.e., jobs
	// that are enqueued prior to the job under analysis) and boost
	// blocking, which occurs when the job under analysis is delayed
	// because some other job is priority-boosted. Boost blocking can be
	// local and transitive from remote CPUs. To compute this correctly,
	// we need to count how many times some job on a remote CPU can directly
	// block the job under analysis. So we first compute direct blocking
	// and count on which CPUs a job can be blocked.

	unsigned int i;

	// direct blocking results
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		std::vector<Interference> counts(resources.size());
		Interference remote, local;

		// Determine counts.
		pfmlp_count_direct_blocking(&tsk, resources, counts);

		// Find longest remote requests.
		remote = pfmlp_bound_remote_blocking(&tsk, access_counts[i], counts,
							 contention);

		// Add in local boost blocking.
		local = pfmlp_bound_local_blocking(&tsk, counts, contention);

		if (!preemptive)
		{
			// Charge for additional delays due to remot non-preemptive
			// sections.
			remote += pfmlp_bound_np_blocking(&tsk, counts, per_cluster);
		}
		results[i] = remote + local;
		results.set_remote_blocking(i, remote);
		results.set_local_blocking(i, local);
	}

	return _results;
}

