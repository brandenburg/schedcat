#ifndef LP_ANALYSYS_H_
#define LP_ANALYSYS_H_

#include "sharedres_types.h"

BlockingBounds* lp_dpcp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality, bool use_RTA = true);

BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality);

BlockingBounds* lp_mpcp_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_part_fmlp_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_omip_bounds(
	const ResourceSharingInfo& info,
	unsigned int num_procs,
	unsigned int cluster_size);

BlockingBounds* lp_msrp_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_preemptive_fifo_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_unordered_bounds(const ResourceSharingInfo& info, bool preemptive = false);

BlockingBounds* lp_baseline_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_prio_bounds(const ResourceSharingInfo& info, bool preemptive = false);

BlockingBounds* lp_prio_fifo_bounds(const ResourceSharingInfo& info, bool preemptive = false);

#endif /* LP_ANALYSYS_H_ */
