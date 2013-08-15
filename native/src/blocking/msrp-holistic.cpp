#include "stl-hashmap.h"

#include "sharedres.h"
#include "blocking.h"


// Analysis of the MSRP: PCP/SRP for local resources, task-fair mutex
// spin locks for global resources
// Applies only to partitioned scheduling.
BlockingBounds* msrp_bounds_holistic(
	const ResourceSharingInfo& info,
	int dedicated_irq)
{
	ResourceSet locals = get_local_resources(info);
	ResourceSharingInfo linfo = extract_local_resources(info, locals);
	ResourceSharingInfo ginfo = extract_global_resources(info, locals);

	BlockingBounds pcp = pcp_blocking(linfo);

	BlockingBounds* results = task_fair_mutex_bounds(ginfo, 1, dedicated_irq);

	// merge the two analysis results
	for (unsigned int i = 0; i < results->size(); i++)
	{
		unsigned int b_pcp  = pcp.get_blocking_term(i);
		unsigned int b_spin = results->get_arrival_blocking(i);

		if (b_pcp > b_spin) {
			// need to account for larger local blocking
			Interference new_arrival(b_pcp);
			Interference total = (*results)[i];
			// increase total by difference to spin-only blocking
			total.total_length += (b_pcp - b_spin);

			// update
			results->set_arrival_blocking(i, new_arrival);
			(*results)[i] = total;
		}
	}

	return results;
}


BlockingBounds pcp_blocking(const ResourceSharingInfo& info)
{
	PriorityCeilings prio_ceilings = get_priority_ceilings(info);

	// split everything by partition
	Clusters clusters;
	split_by_cluster(info, clusters);

	// blocking results
	BlockingBounds results(info);

	foreach(clusters, ct)
	{
		Cluster& cluster = *ct;
		foreach(cluster, it)
		{
			const TaskInfo* tsk = *it;
			unsigned int id     = tsk->get_id();
			unsigned int prio   = tsk->get_priority();

			// check each other task
			foreach(cluster, jt)
			{
				const TaskInfo* other = *jt;
				if (id != other->get_id() &&
				    prio <= other->get_priority())
				{
					// blocking possible

					foreach(other->get_requests(), req)
					{
						unsigned int res = req->get_resource_id();
						if (prio_ceilings[res] <= prio) {
							// this CS could cause ceiling blocking / PI blocking
							// make sure blocking term is at least this large
							Interference inf(req->get_request_length());
							results.raise_blocking_length(id, inf);
						}
					}
				}
			}
		}
	}

	return results;
}
