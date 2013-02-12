#ifndef _MPCP_H_
#define _MPCP_H_

typedef std::vector<unsigned long> ResponseTimes;
typedef std::vector<ResponseTimes> TaskResponseTimes;
typedef std::vector<TaskResponseTimes> ClusterResponseTimes;

typedef std::vector<PriorityCeilings> MPCPCeilings;

void determine_gcs_response_times(const Clusters& clusters,
				  const MPCPCeilings& ceilings,
				  ClusterResponseTimes& times);

MPCPCeilings get_mpcp_ceilings(const ResourceSharingInfo& info);

#endif
