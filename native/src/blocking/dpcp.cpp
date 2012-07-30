#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"

// ************************************************** DPCP **************
/*

  DPCP blocking terms (Rajkumar, 1991, page 87):

  1) Local PCP blocking => does not apply here, we only care about global
  resources.

  2) A lower-priority gcs on a remote proc each time that Ji issues a request.

  3) All requests of all higher-priority tasks on all remote processors that Ji
  accesses.

  4) Global critical sections on Ji's CPU. Since gcs are not part of the job
  execution time in our model, it does not matter whether the local gcs's
  belong to lower or higher-priority tasks.
 */


static void split_by_locality(const ResourceSharingInfo& info,
			      const ResourceLocality& locality,
			      AllPerCluster& per_cluster)
{
	foreach(info.get_tasks(), it)
	{
		while (it->get_cluster()  >= per_cluster.size())
			per_cluster.push_back(ContentionSet());

		foreach(it->get_requests(), jt)
		{
			const RequestBound &req = *jt;
			int cpu = locality[req.get_resource_id()];

			if (cpu == NO_CPU)
				// NO_CPU => dedicated synchronization processor
				continue;

			while ((unsigned int) cpu  >= per_cluster.size())
				per_cluster.push_back(ContentionSet());

			per_cluster[cpu].push_back(&req);
		}
	}
}

static unsigned int count_requests_to_cpu(
	const TaskInfo& tsk,
	const ResourceLocality& locality,
	int cpu)
{
	unsigned int count = 0;

	foreach(tsk.get_requests(), req)
		if (locality[req->get_resource_id()] == cpu)
			count += req->get_num_requests();

	return count;
}

static Interference bound_blocking_dpcp(
	const TaskInfo* tsk,
	const ContentionSet& cont,
	const PriorityCeilings& prio_ceiling,
	unsigned int max_lower_prio)
{
	Interference inter;
	const unsigned int interval = tsk->get_response();

	// assumption: cont is ordered by request length
	foreach(cont, it)
	{
		const RequestBound* req = *it;

		// can't block itself
		if (req->get_task() != tsk)
		{
			unsigned int num;
			if (req->get_task()->get_priority() < tsk->get_priority())
			{
				// higher prio => all of them
				num = req->get_max_num_requests(interval);
				inter.count += num;
				inter.total_length += num * req->get_request_length();
			}
			else if (max_lower_prio &&
			         prio_ceiling[req->get_resource_id()] <= tsk->get_priority())
			{
				// lower prio => only remaining
				num = std::min(req->get_max_num_requests(interval), max_lower_prio);
				inter.count += num;
				inter.total_length += num * req->get_request_length();
				max_lower_prio -= num;
			}
		}
	}

	return inter;
}

static Interference dpcp_remote_bound(
	const TaskInfo& tsk,
	const ResourceLocality& locality,
	const PriorityCeilings& prio_ceilings,
	const AllPerCluster& per_cpu)
{
	Interference blocking;
	unsigned int cpu = 0;

	foreach(per_cpu, it)
	{
		// this is about remote delays
		if (cpu != tsk.get_cluster())
		{
			const ContentionSet &cs = *it;
			unsigned int reqs;
			reqs = count_requests_to_cpu(tsk, locality, cpu);

			if (reqs > 0)
				blocking += bound_blocking_dpcp(&tsk, cs, prio_ceilings, reqs);
		}
		cpu++;
	}

	return blocking;
}


static Interference dpcp_local_bound(
	const TaskInfo* tsk,
	const ContentionSet& local)
{
	Interference blocking;
	const unsigned int interval = tsk->get_response();

	foreach(local, it)
	{
		const RequestBound* req = *it;
		if (req->get_task() != tsk)
		{
			unsigned int num;
			num = req->get_max_num_requests(interval);
			blocking.count += num;
			blocking.total_length += num * req->get_request_length();
		}
	}

	return blocking;
}


BlockingBounds* dpcp_bounds(const ResourceSharingInfo& info,
			    const ResourceLocality& locality)
{
	AllPerCluster per_cpu;

	split_by_locality(info, locality, per_cpu);
	sort_by_request_length(per_cpu);

	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;

	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference remote, local;

		remote = dpcp_remote_bound(tsk, locality, prio_ceilings, per_cpu);
		local = dpcp_local_bound(&tsk, per_cpu[tsk.get_cluster()]);

		results[i] = remote + local;
		results.set_remote_blocking(i, remote);
		results.set_local_blocking(i, local);
	}
	return _results;
}

