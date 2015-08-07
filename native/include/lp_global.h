#ifndef LP_GLOBAL_H
#define LP_GLOBAL_H

#include "linprog/varmapperbase.h"

class GlobalVarMapper : public VarMapperBase
{
	enum variable_type_t
	{
		BLOCKING_FRACTION  = 0,
		INTERFERENCE_BOUND = 1
	};

	enum blocking_type_t
	{
		DIRECT_BLOCKING     = 0,
		INDIRECT_BLOCKING   = 1,
		PREEMPTION_BLOCKING = 2,
		EXPELLING_BLOCKING  = 3,

		REGULAR_INTERFERENCE = 0,
		CO_BOOSTING_INTERFERENCE = 1,
		STALLING_INTERFERENCE = 2
	};

	union lookup_key_t
	{
		uint64_t raw;
		struct
		{
			uint64_t tid:20; // task ID
			uint64_t rid:20; // resource ID
			uint64_t xid:20; // request ID

			uint64_t blocking_type:2; // direct, indirect, preempt, expelling
			uint64_t variable_type:1; // blocking fraction or INTERFERENCE_BOUND
		} var;

		enum
		{
			KEY_MAX   = (unsigned) (1 << 20),
			BTYPE_MAX = (unsigned) (1 <<  2),
		};

		/* construct an X^{D,I,P,E} variable */
		void make_var_for(
			unsigned int task_id, unsigned int res_id, unsigned int req_id,
			blocking_type_t btype)
		{
			assert(task_id < KEY_MAX);
			assert(res_id < KEY_MAX);
			assert(req_id < KEY_MAX);

			raw = 0;
			var.tid = task_id;
			var.rid = res_id;
			var.xid = req_id;
			var.blocking_type = btype;
			var.variable_type = BLOCKING_FRACTION;
		}

		/* construct an I variable */
		void make_interference_var_for(
				unsigned int task_id,

				blocking_type_t btype
				)
		{
			assert(task_id < KEY_MAX);

			raw = 0;
			var.tid = task_id;
			var.blocking_type = btype;
			var.variable_type = INTERFERENCE_BOUND;
		}

	};

public:
	unsigned int direct(unsigned int task_id, unsigned int res_id,
	                    unsigned int cs_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, cs_id, DIRECT_BLOCKING);
		return var_for_key(k.raw);
	}

	unsigned int indirect(unsigned int task_id, unsigned int res_id,
	                      unsigned int cs_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, cs_id, INDIRECT_BLOCKING);
		return var_for_key(k.raw);
	}

	unsigned int preemption(unsigned int task_id, unsigned int res_id,
	                        unsigned int cs_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, cs_id, PREEMPTION_BLOCKING);
		return var_for_key(k.raw);
	}

	unsigned int expelling(unsigned int task_id, unsigned int res_id,
	                       unsigned int cs_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, cs_id, EXPELLING_BLOCKING);
		return var_for_key(k.raw);
	}

	unsigned int regular_interference(unsigned int task_id)
	{
		lookup_key_t k;

		k.make_interference_var_for(task_id, REGULAR_INTERFERENCE);
		return var_for_key(k.raw);
	}

	unsigned int co_boosting_interference(unsigned int task_id)
	{
		lookup_key_t k;

		k.make_interference_var_for(task_id, CO_BOOSTING_INTERFERENCE);
		return var_for_key(k.raw);
	}

	unsigned int stalling_interference(unsigned int task_id)
	{
		lookup_key_t k;

		k.make_interference_var_for(task_id, STALLING_INTERFERENCE);
		return var_for_key(k.raw);
	}

	std::string key2str(uint64_t key, unsigned int var) const;
};


class GlobalSuspensionAwareLP : protected LinearProgram
{

protected:
	GlobalVarMapper vars;

	const unsigned int i;
	const TaskInfo& ti;
	const TaskInfos& taskset;
	const unsigned int m;

	const std::set<unsigned int> all_resources;

	// floating point stability threshold
	const static double EPSILON;

protected:
	virtual unsigned long resource_hold_time(unsigned int tx_id, unsigned int res_id) = 0;

	virtual void add_constraints_post_ctor() {};

private:

	void set_objective();

	//Generic constraints
	// Constraint 1
	void add_workload_constraints();

	// Constraint 2
	void add_slack_constraints();

	// Constraint 3
	void add_generic_mutex_pi_blocking_constraints();

	// Constraint 4
	void add_stalling_interference_for_independent_tasks();

	// Constraint 5
	void add_generic_non_access_direct_constraints();

public:
	typedef const unsigned int var_t;

	GlobalSuspensionAwareLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);

	unsigned long solve();
	unsigned long solve_debug();

	// set bounds of interference variables properly, ie, not upper-bounded
	void declare_interference_variables();
};


class GlobalPrioInheritanceLP : virtual public GlobalSuspensionAwareLP
{
private:

	//	------------- PI --------------
	// Constraint 6
	void add_pi_no_co_boosting_interference();

	// Constraint 7
	void add_pi_m_highest_constraint();

protected:
	// Constraint 11
	void add_pip_fmlp_no_stalling_interference();

	// Constraint 12
	void add_pip_ppcp_indirect_preemption_constraints();

	unsigned long resource_hold_time(unsigned int tx_id, unsigned int res_id);

	const PriorityCeilings prio_ceilings;

public:
	GlobalPrioInheritanceLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);
};


class GlobalRestrictedSegmentBoostingLP : virtual public GlobalSuspensionAwareLP
{
private:
	//	------------- RSB --------------
	// Constraint 16
	void add_rsb_co_boosting_stalling_interference_to_csl();

	// Constraint 17
	void add_rsb_total_co_boosting_stalling_interference();

	// Constraint 18
	void add_rsb_co_boosting_interference();

	// Constraint 19
	void add_rsb_total_co_boosting_interference();

	// Constraint 20
	void add_rsb_no_stalling_interference();

	// Constraint 21
	void add_rsb_m_highest_constraint();

	// Constraint 22
	void add_rsb_indirect_constraint();

protected:
	unsigned long resource_hold_time(unsigned int tx_id, unsigned int res_id);

public:
	GlobalRestrictedSegmentBoostingLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);
};


class GlobalNoProgressMechanismLP : virtual public GlobalSuspensionAwareLP
{
private:
	//	---------- no-progress -----------
	// Constraint 14
	void add_no_progress_constraints();

	// Constraint 15
	void add_no_progress_no_stalling_interference();

protected:
	unsigned long resource_hold_time(unsigned int tx_id, unsigned int res_id);

public:
	GlobalNoProgressMechanismLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);
};


class GlobalPriorityQueuesLP : virtual public GlobalSuspensionAwareLP
{
private:
	//	------- priority queuing -----------
	// Constraint 9
	void add_prio_lower_direct_constraints();

	// Constraint 10
	void add_prio_higher_direct_constraints();

protected:
	void add_constraints_post_ctor();

private:
	unsigned long wait_lower_prio(unsigned int res_id);

	unsigned long wait_higher_prio(
		unsigned int res_id,
		unsigned long interval);

	unsigned int higher_direc_num_req(
		unsigned int tx_id,
		unsigned int res_id);

protected:

	//Bounding the maximum time that Ti spends on
	//waiting for the resource with id "res_id"
	unsigned long resource_wait_time(
		unsigned int res_id);

public:
	GlobalPriorityQueuesLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);
};


class GlobalFIFOQueuesLP : virtual public GlobalSuspensionAwareLP
{
private:
	//	------- FIFO queuing --------------
	// Constraint 8
	void add_fifo_direct_constraints();

public:
	GlobalFIFOQueuesLP(
		const ResourceSharingInfo& info,
		unsigned int task_index,
		unsigned int number_of_cpus);
};


#endif
