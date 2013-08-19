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

	// Analyze blocking due to local resources.
	BlockingBounds pcp = pcp_blocking(linfo);

	// Analyze blocking due to global resources.
	BlockingBounds* results = task_fair_mutex_bounds(ginfo, 1, dedicated_irq);

	// Merge the two analysis results.
	// We only care about local resources if the maximum
	// arrival blocking due to local resources exceeds the
	// maximum arrival blocking due to non-preemptive sections
	// as determined by the global analysis.
	for (unsigned int i = 0; i < results->size(); i++)
	{
		// max arrival blocking due to local resource
		unsigned int b_pcp  = pcp.get_blocking_term(i);
		// max arrival blocking due to global resource
		unsigned int b_spin = results->get_arrival_blocking(i);

		if (b_pcp > b_spin) {
			// need to account for larger local blocking
			Interference new_arrival(b_pcp);
			Interference total = (*results)[i];

			// Increase total by difference to spin-only blocking.
			// This is needed because charge_arrival_blocking(),
			// called indirectly via task_fair_mutex_bounds(),
			// also increases the total bound. We patch this up
			// here to correctly reflect the total blocking.
			// NOTE: we are not changing remote blocking,
			//       which still accurately reflects the
			//       maximum time spent spinning.
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
