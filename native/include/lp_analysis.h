#ifndef LP_ANALYSYS_H_
#define LP_ANALYSYS_H_

#include "sharedres_types.h"

BlockingBounds* lp_dpcp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality, bool use_RTA = true);

BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality);

BlockingBounds* lp_mpcp_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_part_fmlp_bounds(const ResourceSharingInfo& info);

#endif /* LP_ANALYSYS_H_ */
