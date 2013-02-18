#ifndef BLOCKING_H
#define BLOCKING_H

#include "stl-hashmap.h"

typedef std::vector<const RequestBound*> ContentionSet;
typedef std::vector<ContentionSet> Resources;
typedef std::vector<Resources> ClusterResources;
typedef std::vector<ContentionSet> AllPerCluster;
typedef std::vector<ContentionSet> TaskContention;
typedef std::vector<TaskContention> ClusterContention;

struct LimitedRequestBound {
	LimitedRequestBound(const RequestBound *rqb, unsigned int l) :
		request_bound(rqb), limit(l) {};
	LimitedRequestBound() : request_bound(NULL), limit(0) {};

	const RequestBound  *request_bound;
	unsigned int        limit;
};

typedef std::vector<LimitedRequestBound> LimitedContentionSet;

void sort_by_request_length(LimitedContentionSet &lcs);
void sort_by_request_length(Resources& resources);
void sort_by_request_length(ClusterResources& resources);
void sort_by_request_length(ContentionSet& cs);


typedef std::vector<const TaskInfo*> Cluster;
typedef std::vector<Cluster> Clusters;

void split_by_cluster(const ResourceSharingInfo& info, Clusters& clusters, unsigned int num_cpus = 0);
void split_by_resource(const ResourceSharingInfo& info, Resources& resources);
void split_by_resource(const Cluster& cluster, Resources& resources);
void split_by_resource(const Clusters& clusters, ClusterResources& resources);


Interference bound_blocking(const ContentionSet& cont,
			    unsigned long interval,
			    unsigned int max_total_requests,
			    unsigned int max_requests_per_source,
			    const TaskInfo* exclude_tsk,
			    unsigned int min_priority = 0);

Interference bound_blocking(const ContentionSet& cont,
			    unsigned long interval,
			    unsigned int max_total_requests,
			    unsigned int max_requests_per_source,
			    bool exclude_whole_cluster,
			    const TaskInfo* exclude_tsk);

Interference np_fifo_per_resource(
	const TaskInfo& tsk, const ClusterResources& clusters,
	unsigned int procs_per_cluster,
	unsigned int res_id, unsigned int issued,
	int dedicated_irq = NO_CPU);

void charge_arrival_blocking(const ResourceSharingInfo& info,
			     BlockingBounds& bounds);


struct ClusterLimit
{
	unsigned int max_total_requests;
	unsigned int max_requests_per_source;

	ClusterLimit(unsigned int total, unsigned int src) :
		max_total_requests(total), max_requests_per_source(src) {}
};

typedef std::vector<ClusterLimit> ClusterLimits;

ClusterLimits np_fifo_limits(
	const TaskInfo& tsk, const ClusterResources& clusters,
	unsigned int procs_per_cluster,
	const unsigned int issued,
	int dedicated_irq);

Interference bound_blocking_all_clusters(
	const ClusterResources& clusters,
	const ClusterLimits& limits,
	unsigned int res_id,
	unsigned long interval,
	const TaskInfo* exclude_tsk);

typedef std::vector<unsigned int> PriorityCeilings;

void determine_priority_ceilings(const Resources& resources,
				 PriorityCeilings& ceilings);

PriorityCeilings get_priority_ceilings(const ResourceSharingInfo& info);

typedef hashset<unsigned int> ResourceSet;

ResourceSet get_local_resources(const ResourceSharingInfo& info);

ResourceSharingInfo extract_local_resources(
	const ResourceSharingInfo& info,
	const ResourceSet& locals);

ResourceSharingInfo extract_global_resources(
	const ResourceSharingInfo& info,
	const ResourceSet& locals);

BlockingBounds pcp_blocking(const ResourceSharingInfo& info);

extern const unsigned int UNLIMITED;

#endif
