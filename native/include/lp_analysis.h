#ifndef LP_ANALYSYS_H_
#define LP_ANALYSYS_H_

#include "sharedres_types.h"

extern BlockingBounds* lp_dpcp_bounds(const ResourceSharingInfo& info,
			    const ResourceLocality& locality, bool use_RTA = true);

extern BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
			    const ResourceLocality& locality);

#endif /* LP_ANALYSYS_H_ */
