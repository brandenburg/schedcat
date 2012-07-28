#ifndef RW_BLOCKING_H
#define RW_BLOCKING_H

void split_by_type(const ContentionSet& requests,
		   ContentionSet& reads,
		   ContentionSet& writes);
void split_by_type(const Resources& resources,
		   Resources &reads,
		   Resources &writes);
void split_by_type(const ClusterResources& per_cluster,
		   ClusterResources &reads);
void split_by_type(const ClusterResources& per_cluster,
		   ClusterResources &reads,
		   ClusterResources &writes);

struct RWCount {
	unsigned int res_id;
	unsigned int num_reads;
	unsigned int num_writes;
	unsigned int rlength;
	unsigned int wlength;

	RWCount(unsigned int id) : res_id(id),
				   num_reads(0),
				   num_writes(0),
				   rlength(0),
				   wlength(0)
	{}
};

typedef std::vector<RWCount> RWCounts;

void merge_rw_requests(const TaskInfo &tsk, RWCounts &counts);

#endif
