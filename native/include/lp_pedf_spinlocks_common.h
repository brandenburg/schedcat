#ifndef LP_PEDF_COMMON_H
#define LP_PEDF_COMMON_H

// ------------------------------------------------------------------
// --------------------[ V A R    M A P P E R ]----------------------
// ------------------------------------------------------------------

#include "linprog/varmapperbase.h"
#include "lp_pedf_analysis.h"

class SpinVarMapper : public VarMapperBase
{
	enum variable_type_t
	{
		SPIN_BLOCKING               = 0,
		ARRIVAL_BLOCKING            = 1,
        INDICATOR_ARRIVAL_BLOCKING  = 2,
        CANCELLATIONS               = 3,
	};

	union lookup_key_t
	{
		uint64_t raw;
		struct
		{
			uint64_t tid:20; // task ID
			uint64_t rid:20; // resource ID

			uint64_t variable_type:2;
		} var;

		enum
		{
			KEY_MAX   = (unsigned) (1 << 20),
			VTYPE_MAX = (unsigned) (1 <<  2),
		};

		/* construct an X^{S,A} variable */
		void make_var_for(
			unsigned int task_id, unsigned int res_id,
			variable_type_t btype)
		{
			assert(task_id < KEY_MAX);
			assert(res_id < KEY_MAX);

			raw = 0;
			var.tid = task_id;
			var.rid = res_id;
			var.variable_type = btype;
		}
	};

public:
	unsigned int spin(unsigned int task_id, unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, SPIN_BLOCKING);
		return var_for_key(k.raw);
	}

	unsigned int arrival(unsigned int task_id, unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, ARRIVAL_BLOCKING);
		return var_for_key(k.raw);
	}

    unsigned int indicator_arrival(unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(0, res_id, INDICATOR_ARRIVAL_BLOCKING);
		return var_for_key(k.raw);
	}

    unsigned int cancellations(unsigned int task_id, unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, res_id, CANCELLATIONS);
		return var_for_key(k.raw);
	}

	std::string key2str(uint64_t key, unsigned int var) const;
};

// ------------------------------------------------------------------
// -----------------------------[ L P ]------------------------------
// ------------------------------------------------------------------


class PEDFBlockingAnalysisLP_Spinlocks : protected LinearProgram
{

protected:

	SpinVarMapper vars;
	const TaskInfos& taskset;

    const ResourceSharingInfo& info;

	// which type of LP are we constructing?
	const analysis_type_t lp_type;

	// length of the analysis interval
	const unsigned long interval_length;

    // cluster under analysis
    unsigned int cluster;

	const std::set<unsigned int> all_resources;

	// Hack that may be needed in derived classes if some
	// constraints need to reference member fields that are not yet initialized
	// during object construction.
	virtual void add_constraints_post_ctor() {};

private:

	void set_objective();

	// Generic constraints: constraints that hold under any locking protocol...

    // Constraint 8
    void add_no_arrival_blocking_dline_inside_interval();

    // Constraint 9
    void add_no_spin_delay_local_requests();

    // Constraint 10
    void add_joint_upper_bound_remote_requests();

    // Constraint 11
    void add_arrival_blocking_single_resource();

    // Constraint 11-bis (AC)
    void add_no_arrival_blocking();

    // Constraint 12
    void add_exclude_non_conflicting_local_resources();

    // Constraint 13
    void add_no_requests_no_arrival_blocking();

    // Constraint 14
    void add_arrival_blocking_max_one_local_request();


public:
	typedef const unsigned int var_t;

	PEDFBlockingAnalysisLP_Spinlocks(
		const ResourceSharingInfo& info,
		analysis_type_t analysis_type,
		unsigned long interval_length,
        unsigned int cluster);

	unsigned long solve(bool verbose = false);
};

#endif