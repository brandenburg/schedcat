#ifndef LP_ANALYSYS_H_
#define LP_ANALYSYS_H_

#include "sharedres_types.h"

/* The following analyses are described in the extended version of:
 *
 *  B. Brandenburg, "Improved Analysis and Evaluation of Real-Time Semaphore
 * Protocols for P-FP Scheduling", RTAS'13.
 */

BlockingBounds* lp_dpcp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality, bool use_RTA = true);

BlockingBounds* lp_dflp_bounds(const ResourceSharingInfo& info,
		    const ResourceLocality& locality);

BlockingBounds* lp_mpcp_bounds(const ResourceSharingInfo& info);

BlockingBounds* lp_part_fmlp_bounds(const ResourceSharingInfo& info);

/* The analysis of the OMIP is described in the extended version of:
 *
 * B. Brandenburg, "A Fully Preemptive Multiprocessor Semaphore Protocol for
 * Latency-Sensitive Real-Time Applications”, ECRTS'13.
 */
BlockingBounds* lp_omip_bounds(
	const ResourceSharingInfo& info,
	unsigned int num_procs,
	unsigned int cluster_size
);

/* The analysis of the Generalized FMLP+ is described in the extended version of:
 *
 * B. Brandenburg, "The FMLP+: An Asymptotically Optimal Real-Time Locking
 * Protocol for Suspension-Aware Analysis”, ECRTS'14.
 */
BlockingBounds* lp_gfmlp_bounds(
	const ResourceSharingInfo& info,
	unsigned int cluster_size,
	bool using_edf
);

/* The following dummy bounds function always returns zero blocking. */
BlockingBounds* dummy_bounds(const ResourceSharingInfo& info);


/* The following analyses are described in the extended version of:
 *
 *  A. Wieder and B. Brandenburg, "On Spin Locks in AUTOSAR: Blocking Analysis
 *  of FIFO, Unordered, and Priority-Ordered Spin Locks", RTSS'13.
 */

/* Analysis of the MSRP under P-FP scheduling */
BlockingBounds* lp_pfp_msrp_bounds(const ResourceSharingInfo& info);

/* Analysis of FIFO spin locks with preemptable spinning under P-FP scheduling */
BlockingBounds* lp_pfp_preemptive_fifo_spinlock_bounds(
	const ResourceSharingInfo& info);

/* Analysis of unordered spin locks with preemptable and non-preemptable
 * spinning under P-FP scheduling */
BlockingBounds* lp_pfp_unordered_spinlock_bounds(
	const ResourceSharingInfo& info,
	bool preemptive = false);

/* Basic analysis without any lock-specific constraints. Useful for comparison
 * purposes only. */
BlockingBounds* lp_pfp_baseline_spinlock_bounds(const ResourceSharingInfo& info);

/* Analysis of priority-ordered spin locks and unordered tie-breaks
 * with preemptable and non-preemptable spinning under P-FP scheduling */
BlockingBounds* lp_pfp_prio_spinlock_bounds(
	const ResourceSharingInfo& info,
	bool preemptive = false);

/* Analysis of priority-ordered spin locks and FIFO-ordered tie-breaks
 * with preemptable and non-preemptable spinning under P-FP scheduling */
BlockingBounds* lp_pfp_prio_fifo_spinlock_bounds(
	const ResourceSharingInfo& info,
	bool preemptive = false);

/* Suspension-aware analysis of the priority inheritance protocol under
 * global scheduling.
 */
BlockingBounds* lp_global_pip_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);

/* Suspension-aware analysis of the parallel priority ceiling protocol (P-PCP)
 * under global scheduling */
BlockingBounds* lp_ppcp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus,
	bool reasonable_priority_assignment = false);

/* Suspension-aware analysis of the flexible multiprocessor locking protocol
 * (FMLP) under global scheduling */
BlockingBounds* lp_sa_gfmlp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);

/* Suspension-aware analysis of the generalized FIFO multiprocessor locking
 * protocol (FMLP+) under global scheduling */
BlockingBounds* lp_global_fmlpp_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);

/* Suspension-aware analysis of the Priority-based Restricted Segment Boosting
 * (PRSB) protocol under global scheduling*/
BlockingBounds* lp_prsb_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);

/* Suspension-aware analysis for no progress mechanism and FIFO queuing under
 * global scheduling */
BlockingBounds* lp_no_progress_fifo_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);

/* Suspension-aware analysis for no progress mechanism and priority queuing
 * under global scheduling */
BlockingBounds* lp_no_progress_priority_bounds(
	const ResourceSharingInfo& info,
	unsigned int number_of_cpus);
#endif /* LP_ANALYSYS_H_ */
