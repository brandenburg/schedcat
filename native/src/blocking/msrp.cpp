#include "sharedres.h"
#include "blocking.h"

#include "stl-helper.h"
#include <set>
#include <iostream>

static Interference msrp_local_bound(const TaskInfo* tsk,
	const Cluster& local,
	const PriorityCeilings& prio_ceilings,
	const std::set<unsigned int>& global_resources);
static Interference msrp_remote_bound(const TaskInfo& tsk,
	Clusters& clusters,
	const std::set<unsigned int>& global_resources,
	unsigned int num_cpus,
	Interference& np_blocking);
static const std::set<unsigned int> get_global_resources(const Resources& res);

void print_ts(const ResourceSharingInfo& info)
{
	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk = info.get_tasks().at(i);
		std::cout<<"task "<<tsk.get_id()<<" uses resources: ";
		for (unsigned int res=0; res < tsk.get_requests().size(); res++)
			std::cout<<tsk.get_requests().at(res).get_resource_id()<<" ";
		std::cout<<"\n";

	}
}

BlockingBounds* msrp_bounds(const ResourceSharingInfo& info, unsigned int num_cpus)
{
	Clusters clusters;
	Resources reqs_per_res;
	split_by_resource(info, reqs_per_res);
	split_by_cluster(info, clusters, num_cpus);
	std::set<unsigned int> global_resources = get_global_resources(reqs_per_res);
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);
	BlockingBounds* _results = new BlockingBounds(info);
	BlockingBounds& results = *_results;
	Interference *np_blocking = new Interference[info.get_tasks().size()];

	// determine remote blocking
	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference remote;
		//ignore tasks on virtual partitions
		if (tsk.get_cluster() < num_cpus)
			remote = msrp_remote_bound(tsk, clusters, global_resources, num_cpus, np_blocking[i]);
		results.set_remote_blocking(i, remote);
	}

	// determine local blocking blocking
	for (unsigned int i = 0; i < info.get_tasks().size(); i++)
	{
		const TaskInfo& tsk  = info.get_tasks()[i];
		Interference local;
		unsigned long max_np_blocking = 0;

		// ignore tasks on virtual partitions
		if (tsk.get_cluster() < num_cpus)
		{
			//find maximal NP-blocking of tasks on the same cluster
			for (unsigned int j = 0; j < info.get_tasks().size(); j++)
			{
				const TaskInfo& tx  = info.get_tasks()[j];     // NP-blocking can be caused by tasks..
				if (!(tx.get_cluster() != tsk.get_cluster()    // ..on the same partition..
					|| tx.get_priority() < tsk.get_priority()  // ..with lower priority
					|| i == j))
					max_np_blocking = std::max(max_np_blocking, np_blocking[j].total_length);
			}
			// determine local blocking
			local = msrp_local_bound(&tsk, clusters[tsk.get_cluster()], prio_ceilings, global_resources);
		}
		// set local blocking term to NP-blocking if higher than regular local blocking
		local.total_length = std::max(local.total_length, max_np_blocking);
		results[i] = results.get_raw_remote_blocking(i) + local;
		results.set_local_blocking(i, local);
	}
	delete[] np_blocking;
	return _results;
}

// determine all global resource, i.e., resources that
// are accessed by tasks that are on different clusters
static const std::set<unsigned int> get_global_resources(const Resources& res)
{
	std::set<unsigned int> global_resources;
	for (unsigned int r=0; r < res.size(); r++)
	{
		std::set<unsigned int> clusters_using_r;
		for (unsigned int t = 0; t < res[r].size(); t++)
		{
			const RequestBound* rb = res[r][t];
			const TaskInfo* tsk = rb->get_task();
			unsigned int cluster = tsk->get_cluster();
			clusters_using_r.insert(cluster);
			if (clusters_using_r.size() > 1)
			{
				global_resources.insert(r);
				break;
			}
		}
	}
	return global_resources;
}

// compute remote blocking
static Interference msrp_remote_bound(
	const TaskInfo& tsk,
	Clusters& clusters,
	const std::set<unsigned int>& global_resources,
	unsigned int num_cpus, Interference& np_blocking)
{
	Interference blocking;

	for (unsigned int res=0; res < tsk.get_requests().size(); res++)
	{
		unsigned int res_id = tsk.get_requests().at(res).get_resource_id();
		if (!global_resources.count(res_id)) // ignore non-global resources
			continue;
		unsigned long max_csl_sum = 0;  // sum of maximal CSLs of each partition
		for (unsigned int cpu=0; cpu<num_cpus; cpu++) // For all partitions..
		{
			if (cpu != tsk.get_cluster()) // ..except for the current task's own partition..
			{
				const Cluster &c = clusters.at(cpu);
				unsigned int max_csl = 0; // max CSL of any task on partition /cpu/ accessing /res/
				for (unsigned int t = 0; t < c.size(); t++) // ..iterate over all tasks..
				{
					Requests reqs = c.at(t)->get_requests();
					for (unsigned int req = 0; req < reqs.size(); req++) // .. and their requests..
					{
						if (reqs.at(req).get_resource_id() == res_id) // ..to res..
						{
							unsigned int csl = reqs.at(req).get_request_length();
							max_csl = std::max(max_csl, csl); // ..and update the max. CSL.
						}
					}
				}
				max_csl_sum += max_csl;
			}
		}
		blocking.count += tsk.get_requests().at(res).get_num_requests();
		blocking.total_length += tsk.get_requests().at(res).get_num_requests() * max_csl_sum;
		// keep track of the maximal NP-blocking that the current task incurs
		np_blocking.total_length = std::max(np_blocking.total_length, max_csl_sum + tsk.get_requests().at(res).get_request_length());
	}
	return blocking;
}

static Interference msrp_local_bound(
	const TaskInfo* tsk,
	const Cluster& local,
	const PriorityCeilings& prio_ceilings,
	const std::set<unsigned int>& global_resources)
{
	Interference blocking;

	unsigned int max_csl = 0;
	// iterate over all requests issued by local tasks
	for (unsigned int t = 0; t < local.size(); t++)
	{
		Requests reqs = local.at(t)->get_requests();
		for (unsigned int r=0; r<reqs.size(); r++)
		{
			const RequestBound& req = reqs.at(r);
			unsigned int res_id = req.get_resource_id();
			if (req.get_task()->get_priority() <= tsk->get_priority()  // ignore tasks of same and higher prio (including self)
					|| global_resources.count(res_id) > 0              // ignore global resources
					|| prio_ceilings.at(res_id) > tsk->get_priority()) // ignore req.s to resources with ceiling too low to block us
				continue;
			max_csl = std::max(max_csl, req.get_request_length()); // update max. CSL of request that can block us
		}
	}
	if (max_csl > 0)
	{
		blocking.count = 1;
		blocking.total_length = max_csl;
	}
	return blocking;
}
