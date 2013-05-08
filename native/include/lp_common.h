#ifndef LP_COMMON_H_
#define LP_COMMON_H_

#include <stdint.h>

#include "sharedres_types.h"
#include "blocking.h"

#include "stl-helper.h"
#include "stl-hashmap.h"

#include "linprog/model.h"
#include "linprog/solver.h"

enum blocking_type
{
	BLOCKING_DIRECT,
	BLOCKING_INDIRECT,
	BLOCKING_PREEMPT,
	BLOCKING_OTHER
};

// s-oblivious analysis reuses BLOCKING_DIRECT as a catch-all blocking type.
#define BLOCKING_SOB BLOCKING_DIRECT

class VarMapper {
private:
	hashmap<uint64_t, unsigned int> map;
	unsigned int next_var;
	bool sealed;

	void insert(uint64_t key)
	{
		assert(next_var < UINT_MAX);
		assert(!sealed);

		unsigned int idx = next_var++;
		map[key] = idx;
	}

	static uint64_t encode_request(uint64_t task_id, uint64_t res_id, uint64_t req_id,
		                       uint64_t blocking_type)
	{
		assert(task_id < (1 << 30));
		assert(res_id < (1 << 10));
		assert(req_id < (1 << 22));
		assert(blocking_type < (1 << 2));

		return (blocking_type << 62) | (task_id << 30) | (req_id << 10) | res_id;
	}

public:
	VarMapper(unsigned int start_var = 0)
		: next_var(start_var), sealed(false)
	{}

	unsigned int lookup(unsigned int task_id, unsigned int res_id, unsigned int req_id,
	                    blocking_type type)
	{
		uint64_t key = encode_request(task_id, res_id, req_id, type);
		if (!map.count(key))
			insert(key);
		return map[key];
	}

	// stop new IDs from being generated
	void seal()
	{
		sealed = true;
	}

	void clear()
	{
		map.clear();
	}

	unsigned int get_num_vars() const
	{
		return map.size();
	}

	unsigned int get_next_var() const
	{
		return next_var;
	}
};

// spinlock analysis: re-use indirect for arrival
#define BLOCKING_ARRIVAL BLOCKING_INDIRECT

class VarMapperSpinlocks : public VarMapper
{
public:
	VarMapperSpinlocks(unsigned int start_var = 0)
		: VarMapper(start_var) { }
	// re-use preemption blocking for arrival blocking decision variables
	unsigned int lookup_arrival_enabled(unsigned int res_id)
	{
		return lookup(0, res_id, 0, BLOCKING_PREEMPT);
	}

	unsigned int lookup_max_preemptions(unsigned int res_id)
	{
		return lookup(0, res_id, 0, BLOCKING_OTHER);
	}
};

void set_blocking_objective(
	VarMapper& vars,
	const ResourceSharingInfo& info, const ResourceLocality&,
	const TaskInfo& ti,
	LinearProgram& lp,
	LinearExpression *local_obj = 0,
	LinearExpression *remote_obj = 0);

void set_blocking_objective_part_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp,
	LinearExpression *local_obj = 0,
	LinearExpression *remote_obj = 0);

void set_blocking_objective_sob(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

void add_mutex_constraints(VarMapper& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,  LinearProgram& lp);

void add_topology_constraints(VarMapper& vars,
		const ResourceSharingInfo& info, const ResourceLocality& locality,
		const TaskInfo& ti, LinearProgram& lp);

void add_local_lower_priority_constraints(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const ResourceLocality& locality,
	const TaskInfo& ti,
	LinearProgram& lp);

void add_topology_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

void add_local_lower_priority_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);

void add_local_higher_priority_constraints_shm(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	LinearProgram& lp);


// common constraints and helper functions for spinlocks analysis
void add_common_spinlock_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp);

void add_common_preemptive_spinlock_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp);

unsigned int count_local_hp_reqs(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id);

unsigned int count_requests_while_pending(
		const ResourceSharingInfo& info,
		unsigned long period,
		unsigned int res_id,
		unsigned int cluster);

void set_spinlock_blocking_objective(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp);

unsigned int get_min_prio(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		bool LP);

unsigned long get_hp_interference(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		const unsigned long interval);


// used by both non-preemptive unordered prioritized mutex spinlocks and
// prioritized FIFO mutex spinlocks
void add_prio_blocking_LP_constraints(
		VarMapperSpinlocks& vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp,
		bool preemptive);

void add_preemptive_fifo_max_preempt_constraints(
		VarMapperSpinlocks & vars,
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		LinearProgram& lp);

unsigned int max_preemptions(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned long interval = 0);

unsigned long get_LPlh(
		const ResourceSharingInfo& info,
		const TaskInfo& ti,
		unsigned int res_id,
		unsigned int locking_prio,
		unsigned long W,
		std::set<unsigned int>& Qlh);

unsigned int get_min_prio(
		const TaskInfo& ti,
		unsigned int res_id);

unsigned int get_max_reqs(
		const TaskInfo& ti,
		unsigned int res_id);

std::set<unsigned int> get_all_resources(const ResourceSharingInfo& info);
std::set<unsigned int> get_global_resources(const ResourceSharingInfo& info);
std::set<unsigned int> get_localHP_resources(const ResourceSharingInfo& info, const TaskInfo& ti);

// A generic for loop that iterates 'request_index_variable' from 0 to the
// maximum number of requests issued by task tx while ti is pending. 'tx_request'
// should be of type RequestBound&.
#define foreach_request_instance(tx_request, task_ti, request_index_variable) \
	for (								\
		unsigned int __max_num_requests = (tx_request).get_max_num_requests((task_ti).get_response()), \
				 request_index_variable = 0;		\
		request_index_variable < __max_num_requests;		\
		request_index_variable++				\
		)

// iterate over each task using 'task_iter', skipping 'excluded_task'
#define foreach_task_except(tasks, excluded_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_id() != (excluded_task).get_id())

// iterate only over tasks with equal or lower priority
#define foreach_lowereq_priority_task(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				      \
	if (task_iter->get_priority() >= (reference_task).get_priority())

// iterate only over tasks with equal or lower priority, excluding 'reference_task'
#define foreach_lowereq_priority_task_except(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				      \
	if (task_iter->get_priority() >= (reference_task).get_priority() &&    \
		task_iter->get_id() != (reference_task).get_id())


// iterate only over tasks with higher priority
#define foreach_higher_priority_task(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				       \
	if (task_iter->get_priority() < (reference_task).get_priority())

// iterate over requests not in the local cluster
#define foreach_remote_request(requests, locality, task_ti, request_iter) \
	foreach(requests, request_iter)					\
	if ((locality)[request_iter->get_resource_id()]			\
	    != (int) (task_ti).get_cluster())

// iterate over requests for resources in a specific cluster
#define foreach_request_in_cluster(requests, locality, cluster, request_iter) \
	foreach(requests, request_iter)					\
	if ((locality)[request_iter->get_resource_id()]			\
	    == (int) (cluster))

// iterate over each task using 'task_iter', skipping tasks in the same
// cluster as 'local_task'
#define foreach_remote_task(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() != (local_task).get_cluster())

// iterate only over tasks with equal or lower priority, excluding local tasks
#define foreach_remote_lowereq_priority_task(tasks, reference_task, task_iter) \
	foreach_remote_task(tasks, reference_task, task_iter)			   \
	if (task_iter->get_priority() >= (reference_task).get_priority())

// iterate only over tasks with higher priority, excluding local tasks
#define foreach_remote_higher_priority_task(tasks, reference_task, task_iter) \
	foreach_remote_task(tasks, reference_task, task_iter)		   \
	if (task_iter->get_priority() < (reference_task).get_priority())

#define foreach_local_task(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster())

#define foreach_local_task_except(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster() && \
	    task_iter->get_id() != (local_task).get_id())

#define foreach_local_lowereq_priority_task_except(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster() && \
	    task_iter->get_id() != (local_task).get_id() && \
	    task_iter->get_priority() >= (local_task).get_priority())

#define foreach_request_for(requests, res_id, req_iter)	\
	foreach(requests, req_iter)				\
	if (req_iter->get_resource_id() == res_id)


#endif /* LP_COMMON_H_ */
