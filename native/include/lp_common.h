#ifndef LP_COMMON_H_
#define LP_COMMON_H_

#include <stdint.h>

#include "sharedres_types.h"
#include "blocking.h"

#include "stl-helper.h"

#include "linprog/model.h"
#include "linprog/solver.h"
#include "linprog/varmapperbase.h"

enum blocking_type
{
	BLOCKING_DIRECT,
	BLOCKING_INDIRECT,
	BLOCKING_PREEMPT,
	BLOCKING_OTHER
};

// s-oblivious analysis reuses BLOCKING_DIRECT as a catch-all blocking type.
#define BLOCKING_SOB BLOCKING_DIRECT

class VarMapper : public VarMapperBase {
private:

	static uint64_t encode_request(uint64_t task_id, uint64_t res_id, uint64_t req_id,
		                       uint64_t blocking_type)
	{
		assert(task_id < (1 << 30));
		assert(res_id < (1 << 10));
		assert(req_id < (1 << 22));
		assert(blocking_type < (1 << 2));

		return (blocking_type << 62) | (task_id << 30) | (req_id << 10) | res_id;
	}

	static uint64_t get_task(uint64_t var)
	{
		return (var >> 30) & (uint64_t) 0x3fffffff;
	}

	static uint64_t get_type(uint64_t var)
	{
		return (var >> 62) & (uint64_t) 0xf;
	}

	static uint64_t get_req_id(uint64_t var)
	{
		return (var >> 10) & (uint64_t) 0xfffff;
	}

	static uint64_t get_res_id(uint64_t var)
	{
		return var & (uint64_t) 0x3ff;
	}

public:
	VarMapper(unsigned int start_var = 0)
		: VarMapperBase(start_var)
	{}

	unsigned int lookup(unsigned int task_id, unsigned int res_id, unsigned int req_id,
	                    blocking_type type)
	{
		uint64_t key = encode_request(task_id, res_id, req_id, type);
		return var_for_key(key);
	}

	std::string key2str(uint64_t key, unsigned int var) const;
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

#include "iter-helper.h"

#endif /* LP_COMMON_H_ */
