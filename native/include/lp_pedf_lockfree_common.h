#ifndef LP_PEDF_LOCKFREE_COMMON_H
#define LP_PEDF_LOCKFREE_COMMON_H

// ------------------------------------------------------------------
// --------------------[ V A R    M A P P E R ]----------------------
// ------------------------------------------------------------------

#include "linprog/varmapperbase.h"
#include "lp_pedf_analysis.h"

class LockFreeVarMapper : public VarMapperBase
{
	enum variable_type_t
	{
		LOCAL_CONFLICT              = 0,
		REMOTE_CONFLICT             = 1,
        INDICATOR_ARRIVAL_BLOCKING  = 2,
	};

	union lookup_key_t
	{
		uint64_t raw;
		struct
		{
			uint64_t tid_i:20; // task i ID
            uint64_t tid_j:20; // task j ID
			uint64_t rid:20; // resource ID

			uint64_t variable_type:2;
		} var;

		enum
		{
			KEY_MAX   = (unsigned) (1 << 20),
			VTYPE_MAX = (unsigned) (1 <<  2),
		};

		/* construct an Y^{L,R} variable */
		void make_var_for(
			unsigned int task_i_id, unsigned int task_j_id,
            unsigned int res_id, variable_type_t btype)
		{
			assert(task_i_id < KEY_MAX);
            assert(task_j_id < KEY_MAX);
			assert(res_id < KEY_MAX);

			raw = 0;
			var.tid_i = task_i_id;
            var.tid_j = task_j_id;
			var.rid = res_id;
			var.variable_type = btype;
		}
	};

public:
	unsigned int local_conflicts(unsigned int task_i_id, unsigned int task_j_id,
                                unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_i_id, task_j_id, res_id, LOCAL_CONFLICT);
		return var_for_key(k.raw);
	}

	unsigned int remote_conflicts(unsigned int task_id, unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, 0, res_id, REMOTE_CONFLICT);
		return var_for_key(k.raw);
	}

    unsigned int indicator_arrival(unsigned int task_id, unsigned int res_id)
	{
		lookup_key_t k;

		k.make_var_for(task_id, 0, res_id, INDICATOR_ARRIVAL_BLOCKING);
		return var_for_key(k.raw);
	}

	std::string key2str(uint64_t key, unsigned int var) const;
};

// ------------------------------------------------------------------
// -----------------------------[ L P ]------------------------------
// ------------------------------------------------------------------


class PEDFBlockingAnalysisLP_LockFree : protected LinearProgram
{

protected:

	LockFreeVarMapper vars;
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

    bool integer_relaxation;

    void add_no_arrival_blocking();

private:

	void set_objective();

	// Generic constraints:
    void add_blocking_upper_and_lower_bound(unsigned long blocking_LB,
                                            unsigned long blocking_UB);
    void add_no_retries_for_resources_not_accessed();
    void add_one_retry_for_at_most_one_remote_commit();


public:
	typedef const unsigned int var_t;

	PEDFBlockingAnalysisLP_LockFree(
		const ResourceSharingInfo& info,
		analysis_type_t analysis_type,
		unsigned long interval_length,
        unsigned int cluster,
        unsigned long blocking_LB,
        unsigned long blocking_UB = 0,
        bool relax = true);

	unsigned long solve(bool verbose = false);
};

#endif
