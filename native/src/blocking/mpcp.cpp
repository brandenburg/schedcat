#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"
#include "math-helper.h"

# include "mpcp.h"

static void determine_mpcp_ceilings(const Resources& resources,
				    const unsigned int cluster,
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
			// count only requests of tasks on remote clusters
			if (req->get_task()->get_cluster() != cluster)
			{
				ceiling = std::min(ceiling, req->get_task()->get_priority());
			}
		}

		ceilings.push_back(ceiling);
	}
}

MPCPCeilings get_mpcp_ceilings(const ResourceSharingInfo& info)
{
	Resources resources;
	Clusters clusters;
	MPCPCeilings ceilings;
	unsigned int cluster;

	split_by_resource(info, resources);
	split_by_cluster(info, clusters);

	enumerate(clusters, it, cluster)
	{
		ceilings.push_back(PriorityCeilings());
		determine_mpcp_ceilings(resources, cluster, ceilings.back());
	}
	return ceilings;
}


// ***************************  MPCP ******************************************

static unsigned long get_max_gcs_length(const TaskInfo* tsk,
					const MPCPCeilings& ceilings,
					unsigned int preempted_ceiling)
{
	unsigned long gcs_length = 0;

	foreach(tsk->get_requests(), it)
	{
		unsigned int prio  = ceilings[it->get_task()->get_cluster()][it->get_resource_id()];
		if (prio <= preempted_ceiling)
			gcs_length = std::max(gcs_length,
					      (unsigned long) it->get_request_length());
	}

	return gcs_length;
}

static void determine_gcs_response_times(const TaskInfo* tsk,
					 const Cluster& cluster,
					 const MPCPCeilings& ceilings,
					 ResponseTimes& times)
{
	times.reserve(tsk->get_requests().size());

	foreach(tsk->get_requests(), it)
	{
		unsigned long resp = it->get_request_length();
		unsigned int prio  = ceilings[it->get_task()->get_cluster()][it->get_resource_id()];

		// Equation (2) in LNR:09.
		// One request of each local gcs that can preempt our ceiling,
		// but at most one per task (since tasks are sequential).

		foreach(cluster, jt)
		{
			const TaskInfo* t = *jt;

			if (t != tsk)
				resp += get_max_gcs_length(t, ceilings, prio);
		}

		times.push_back(resp);
	}
}

static void determine_gcs_response_times(const Cluster& cluster,
					 const MPCPCeilings& ceilings,
					 TaskResponseTimes& times)
{
	times.reserve(cluster.size());
	foreach(cluster, it)
	{
		times.push_back(ResponseTimes());
		determine_gcs_response_times(*it, cluster, ceilings,
					     times.back());
	}
}

void determine_gcs_response_times(const Clusters& clusters,
				  const MPCPCeilings& ceilings,
				  ClusterResponseTimes& times)
{
	times.reserve(clusters.size());
	foreach(clusters, it)
	{
		times.push_back(TaskResponseTimes());
		determine_gcs_response_times(*it, ceilings, times.back());
	}
}

static unsigned long response_time_for(unsigned int res_id,
 				       unsigned long interval,
				       const TaskInfo* tsk,
				       const ResponseTimes& resp,
				       bool multiple)
{
	const Requests& requests = tsk->get_requests();
	unsigned int i = 0;

	for (i = 0; i < requests.size(); i++)
		if (requests[i].get_resource_id() == res_id)
		{
			if (multiple)
			{
				// Equation (3) in LNR:09.
				// How many jobs?
				unsigned long num_jobs;
				num_jobs  = divide_with_ceil(interval, tsk->get_period());
				num_jobs += 1;

				// Note: this may represent multiple gcs, so multiply.
				return num_jobs * resp[i] * requests[i].get_num_requests();
			}
			else
				// Just one request.
				return resp[i];
		}
	// if we get here, then the task does not access res_id
	return 0;
}

static unsigned long  mpcp_remote_blocking(unsigned int res_id,
					   unsigned long interval,
					   const TaskInfo* tsk,
					   const Cluster& cluster,
					   const TaskResponseTimes times,
					   unsigned long& max_lower)
{
	unsigned int i;
	unsigned long blocking = 0;

	// consider each task in cluster
	for (i = 0; i < cluster.size(); i++)
	{
		const TaskInfo* t = cluster[i];
		if (t != tsk)
		{
			if (t->get_priority() < tsk->get_priority())
				// This is a higher-priority task;
				// it can block multiple times.
				blocking += response_time_for(res_id, interval,
							      t, times[i], true);
			else
				// This is a lower-priority task;
				// it can block only once.
				max_lower = std::max(max_lower,
						     response_time_for(res_id, interval,
								       t, times[i], false));
		}
	}

	return blocking;
}

static unsigned long  mpcp_remote_blocking(unsigned int res_id,
					   unsigned long interval,
					   const TaskInfo* tsk,
					   const Clusters& clusters,
					   const ClusterResponseTimes times,
					   unsigned long& max_lower)
{
	unsigned int i;
	unsigned long blocking;

	max_lower = 0;
	blocking  = 0;

	for (i = 0; i < clusters.size(); i++)
	{
		// Note that this also includes the local cluster.
		// This is indeed correct (and matches LNR:09): we
		// are interested in computing the *response time*,
		// which is also affected by local higher-priority tasks.
		// The response-time is used as a bound on blocking.
		blocking += mpcp_remote_blocking(res_id, interval,
						 tsk, clusters[i], times[i],
						 max_lower);
	}
	return blocking;
}

static unsigned long mpcp_remote_blocking(unsigned int res_id,
					  const TaskInfo* tsk,
					  const Clusters& clusters,
					  const ClusterResponseTimes times)
{
	unsigned long interval;
	unsigned long blocking = 1;
	unsigned long max_lower;

	do
	{
		// last bound
		interval = blocking;
		// Bail out if it doesn't converge.
		if (interval > std::max(tsk->get_response(), tsk->get_period()))
			return UNLIMITED;

		blocking = mpcp_remote_blocking(res_id, interval,
						tsk, clusters, times,
						max_lower);

		// Account for the maximum lower-priority gcs
		// that could get in the way.
		blocking += max_lower;

		// Loop until it converges.
	} while ( interval != blocking );

	return blocking;
}

static unsigned long mpcp_remote_blocking(const TaskInfo* tsk,
					  const Clusters& clusters,
					  const ClusterResponseTimes times)
{
	unsigned long blocking = 0;


	const Requests& requests = tsk->get_requests();
	unsigned int i = 0;

	for (i = 0; i < requests.size(); i++)
	{
		unsigned int b;
		b = mpcp_remote_blocking(requests[i].get_resource_id(),
					 tsk, clusters, times);
		if (b != UNLIMITED)
			// may represent multiple, multiply accordingly
			blocking += b * requests[i].get_num_requests();
		else
			// bail out if it didn't converge
			return b;
	}

	return blocking;
}

static unsigned long mpcp_arrival_blocking(const TaskInfo* tsk,
					   const Cluster& cluster,
					   bool virtual_spinning)
{
	unsigned int prio = tsk->get_priority();
	unsigned int blocking = 0;
	unsigned int i;

	for (i = 0; i < cluster.size(); i++)
		if (cluster[i] != tsk && cluster[i]->get_priority() >= prio)
			blocking += cluster[i]->get_max_request_length();

	if (virtual_spinning)
		// Equation (4) in LNR:09.
		return blocking;
	else
		// Equation (1) in LNR:09.
		return blocking * tsk->get_num_arrivals();
}

BlockingBounds* mpcp_bounds(const ResourceSharingInfo& info,
			    bool use_virtual_spinning)
{
	Resources resources;
	Clusters clusters;

	split_by_resource(info, resources);
	split_by_cluster(info, clusters);

	// 2) Determine priority ceiling for each request.
	MPCPCeilings gc = get_mpcp_ceilings(info);


	// 3) For each request, determine response time. This only depends on the
	//    priority ceiling for each request.
	ClusterResponseTimes responses;
	determine_gcs_response_times(clusters, gc, responses);

	unsigned int i;

	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];

		unsigned long remote, local = 0;

		// 4) Determine remote blocking for each request. This depends on the
		//    response times for each remote request.
		remote = mpcp_remote_blocking(&tsk, clusters, responses);

		// 5) Determine arrival blocking for each task.
		local = mpcp_arrival_blocking(&tsk, clusters[tsk.get_cluster()],
					      use_virtual_spinning);

		// 6) Sum up blocking: remote blocking + arrival blocking.
		results[i].total_length = remote + local;


		Interference inf;
		inf.total_length = remote;
		results.set_remote_blocking(i, inf);
		inf.total_length = local;
		results.set_local_blocking(i, inf);
	}

	return _results;
}


