#ifndef _MPCP_H_
#define _MPCP_H_

typedef std::vector<unsigned long> ResponseTimes;
typedef std::vector<ResponseTimes> TaskResponseTimes;
typedef std::vector<TaskResponseTimes> ClusterResponseTimes;

void determine_gcs_response_times(const Clusters& clusters,
				  const PriorityCeilings& ceilings,
				  ClusterResponseTimes& times);
#endif
