from itertools import izip

import schedcat.locking.native as cpp

# The blocking analysis needs to know which task can be preempted by which
# other task. This of course differs under EDF and FP scheduling. To simplify
# the interface, and to avoid having to propagate information about which
# scheduling policy is used to the blocking analysis, we instead determine for
# each task a *preemption level*, with the following interpretation: a task A
# can preempt a task B if and only if A.preemption_level < B.preemption_level.

def assign_edf_preemption_levels(all_tasks):
    all_deadlines = set([t.deadline for t in all_tasks])
    prio = {}
    for i, dl in enumerate(sorted(all_deadlines)):
        prio[int(dl)] = i

    for t in all_tasks:
        t.preemption_level = prio[int(t.deadline)]

def assign_prio_pt_preemption_levels(all_tasks):
    all_prio_pts = set([t.prio_pt for t in all_tasks])
    prio = {}
    for i, pp in enumerate(sorted(all_prio_pts)):
        prio[int(pp)] = i

    for t in all_tasks:
        t.preemption_level = prio[int(t.prio_pt)]

def assign_fp_preemption_levels(all_tasks):
    # prioritized in index order
    for i, t in enumerate(all_tasks):
        t.preemption_level = i

def is_reasonable_priority_assignment(num_cpus, taskset):
    """Check whether the tasks in taskset have been given 'reasonable'
    priorities, according to the def. of Easwaran and Andersson.

    "A priority-assignment to tasks is reasonable if D_i <= D_j for
    every pair (T_i, T_j) such that T_i and T_j are two of the n-m lowest
    base-priority tasks and T_i has higher base-priority than T_j."
    """
    # Look for any two 'neighboring' tasks that do not satisfy the
    # "reasonable order". Ignore the m highest-priority tasks.
    relevant_tasks = taskset[num_cpus:]
    for (ti, tj) in izip(relevant_tasks, relevant_tasks[1:]):
        if ti.deadline > tj.deadline:
            return False
    return True

# assumes mutex constraints
def get_cpp_model(all_tasks, use_task_period=False, use_task_deadline=False, no_requests=False):
    rsi = cpp.ResourceSharingInfo(len(all_tasks))
    for t in all_tasks:
        if use_task_period:
            pending_interval = t.period
        elif use_task_deadline:
            pending_interval = t.deadline
        else:
            pending_interval = t.response_time
        rsi.add_task(t.period,
                     pending_interval,
                     t.partition,
                     t.preemption_level, # mapped to fixed priorities in the C++ code
                     t.cost,
                     t.deadline)
        if not no_requests:
            for req in t.resmodel:
                req = t.resmodel[req]
                if req.max_requests > 0:
                    rsi.add_request(req.res_id, req.max_requests, req.max_length,
                                    req.priority)
    return rsi

def get_cpp_model_rw(all_tasks, use_task_period=False):
    rsi = cpp.ResourceSharingInfo(len(all_tasks))
    for t in all_tasks:
        rsi.add_task(t.period,
                     t.period if use_task_period else t.response_time,
                     t.partition,
                     t.preemption_level, # mapped to fixed priorities in the C++ code
                     t.cost,
                     t.deadline)
        for req in t.resmodel:
            req = t.resmodel[req]
            if req.max_writes > 0:
                rsi.add_request_rw(req.res_id, req.max_writes, req.max_write_length, cpp.WRITE, req.priority)
            if req.max_reads > 0:
                rsi.add_request_rw(req.res_id, req.max_reads, req.max_read_length, cpp.READ, req.priority)
    return rsi

# S-aware bounds

def apply_mpcp_bounds(all_tasks, use_virtual_spin=False):
    model = get_cpp_model(all_tasks)
    res = cpp.mpcp_bounds(model, use_virtual_spin)

    if use_virtual_spin:
        for i,t in enumerate(all_tasks):
            # no suspension time
            t.suspended = 0
            # all blocking, including arrival blocking
            t.sob_blocked = res.get_blocking_term(i)
            # arrival blocking only
            t.blocked = res.get_local_blocking(i)
            # remote blocking increases CPU demand (s-oblivious)
            t.cost += res.get_remote_blocking(i)
    else:
        for i,t in enumerate(all_tasks):
            # remote blocking <=> suspension time
            t.suspended = res.get_remote_blocking(i)
            # all blocking, including arrival blocking
            t.blocked   = res.get_blocking_term(i)
            t.locally_blocked = res.get_local_blocking(i)

    return res

def get_round_robin_resource_mapping(num_resources, num_cpus,
                                     dedicated_irq=cpp.NO_CPU):
    "Default resource assignment: just assign resources to CPUs in index order."
    loc   = {}
    for res_id in xrange(num_resources):
        cpu = res_id % num_cpus
        if cpu == dedicated_irq:
            cpu = (cpu + 1) % num_cpus
        loc[res_id] = cpu
    return loc

def get_cpp_topology(res_mapping):
    map = cpp.ResourceLocality()
    for res_id in res_mapping:
        map.assign_resource(res_id, res_mapping[res_id])
    return map

def apply_dpcp_bounds(all_tasks, resource_mapping,
                      use_text_book_definition=False):
    # The DPCP bounds are expressed in terms of task periods,
    # not response time, in the original definition.
    model = get_cpp_model(all_tasks, use_text_book_definition)
    topo  = get_cpp_topology(resource_mapping)
    res = cpp.dpcp_bounds(model, topo)

    for i,t in enumerate(all_tasks):
        # remote blocking <=> suspension time
        t.suspended = res.get_remote_blocking(i)
        # all blocking, including arrival blocking
        t.blocked   = res.get_blocking_term(i)

    return res

def apply_part_fmlp_bounds(all_tasks, preemptive=True):
    model = get_cpp_model(all_tasks)
    res = cpp.part_fmlp_bounds(model, preemptive)

    for i,t in enumerate(all_tasks):
        # remote blocking <=> suspension time
        t.suspended = res.get_remote_blocking(i)
        # all blocking, including local blocking
        t.blocked   = res.get_blocking_term(i)
        t.local_blocking_count = res.get_local_count(i)

    return res

def apply_classic_pip_bounds(all_tasks, num_cpus):
    # Classic analysis of the PIP under global scheduling
    model = get_cpp_model(all_tasks)
    res = cpp.global_pip_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # hp_blocked <=> blocking workload from higher-priority tasks
        t.blocked  = res.get_blocking_term(i)

        # Hack: we need to know hp_direct_blocked (which Easwaran
        # and Anderson named Ihp_i_dsr) for the response-time analysis.
        # The C++ code is sticking this into the local blocking field,
        # for lack of a better place.
        t.hp_direct_blocked = res.get_local_blocking(i)

    return res

def apply_classic_ppcp_bounds(all_tasks, num_cpus):
    # Classic analysis of the PPCP under global scheduling
    model = get_cpp_model(all_tasks)
    reasonable = is_reasonable_priority_assignment(num_cpus, all_tasks)
    res = cpp.ppcp_bounds(model, num_cpus, reasonable)

    for i,t in enumerate(all_tasks):
         # hp_blocked <=> blocking workload from higher-priority tasks
        t.blocked  = res.get_blocking_term(i)

        # Hack: we need to know hp_direct_blocked (which Easwaran
        # and Anderson named Ihp_i_dsr) for the response-time analysis.
        # The C++ code is sticking this into the local blocking field,
        # for lack of a better place.
        t.hp_direct_blocked = res.get_local_blocking(i)

    return res
# S-oblivious bounds

def apply_suspension_oblivious(all_tasks, res):
    for i,t in enumerate(all_tasks):
        # might be zero
        t.arrival_blocked = res.get_arrival_blocking(i)
        # all blocking, including arrival blocking
        t.sob_blocked   = res.get_blocking_term(i)
        # s-oblivious <=> no suspension, no priority inversion
        t.suspended = 0
        t.blocked   = 0
        # instead, charge it all as execution cost
        t.cost     += t.sob_blocked

def apply_global_fmlp_sob_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = cpp.global_fmlp_bounds(model)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_global_omlp_bounds(all_tasks, num_cpus):
    model = get_cpp_model(all_tasks)
    res = cpp.global_omlp_bounds(model, num_cpus)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_clustered_omlp_bounds(all_tasks, procs_per_cluster,
                                dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model(all_tasks)
    res = cpp.clustered_omlp_bounds(model, procs_per_cluster, dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_clustered_rw_omlp_bounds(all_tasks, procs_per_cluster,
                                   dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model_rw(all_tasks)
    res = cpp.clustered_rw_omlp_bounds(model, procs_per_cluster, dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_clustered_kx_omlp_bounds(all_tasks, procs_per_cluster,
                                   replication_degrees = {},
                                   dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model(all_tasks)
    replica_info = cpp.ReplicaInfo()
    for res_id in replication_degrees:
        replica_info.set_replicas(res_id, replication_degrees[res_id])
    res = cpp.clustered_kx_omlp_bounds(model, replica_info, procs_per_cluster,
                                       dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res

# Spinlocks are either charged as s-oblivious analysis (the default, for legacy
# reasons), or charged in a way such that local priority inversions are
# accounted for explicitly (which is preferable for P-FP).

def apply_pi_aware_spin_inflation(all_tasks, res):
    for i,t in enumerate(all_tasks):
        t.prio_inversion = res.get_arrival_blocking(i)
        t.sob_blocked = res.get_blocking_term(i)
        # PI-aware WCET inflation: charge only spinning as execution cost
        t.cost += t.sob_blocked - t.prio_inversion

def apply_task_fair_mutex_bounds(all_tasks, procs_per_cluster,
                                 dedicated_irq=cpp.NO_CPU, pi_aware=False):
    model = get_cpp_model(all_tasks)
    res = cpp.task_fair_mutex_bounds(model, procs_per_cluster, dedicated_irq)
    if pi_aware:
        apply_pi_aware_spin_inflation(all_tasks, res)
    else:
        apply_suspension_oblivious(all_tasks, res)
    return res

def apply_task_fair_rw_bounds(all_tasks, procs_per_cluster,
                              dedicated_irq=cpp.NO_CPU, pi_aware=False):
    model = get_cpp_model_rw(all_tasks)
    # mutex equivalent model
    model_mtx = get_cpp_model(all_tasks)
    res = cpp.task_fair_rw_bounds(model, model_mtx, procs_per_cluster, dedicated_irq)
    if pi_aware:
        apply_pi_aware_spin_inflation(all_tasks, res)
    else:
        apply_suspension_oblivious(all_tasks, res)
    return res

def apply_phase_fair_rw_bounds(all_tasks, procs_per_cluster,
                                 dedicated_irq=cpp.NO_CPU, pi_aware=False):
    model = get_cpp_model_rw(all_tasks)
    res = cpp.phase_fair_rw_bounds(model, procs_per_cluster, dedicated_irq)
    if pi_aware:
        apply_pi_aware_spin_inflation(all_tasks, res)
    else:
        apply_suspension_oblivious(all_tasks, res)
    return res

### spin lock analysis that assumes priority-inversion-aware schedulability tests
#   (=> FP response-time analysis in schedcat.sched.fp.rta)

def apply_msrp_bounds_holistic(all_tasks, dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model(all_tasks)
    res = cpp.msrp_bounds_holistic(model, dedicated_irq)

    for i, t in enumerate(all_tasks):
        # spin locks => no self-suspension (local blocking is not a self-suspension)
        t.suspended = 0
        # account for local arrival blocking (either local resources or NP section)
        t.blocked   = res.get_arrival_blocking(i)
        # charge only spinning (NOT arrival blocking) as execution time inflation
        t.cost     += res.get_remote_blocking(i)

    return res


### S-aware LP-based analysis of distributed locking protocols

try:
    import schedcat.locking.linprog.native as lp_cpp

    lp_cpp_available = True
except ImportError:
    lp_cpp_available = False

def get_cpp_nested_cs_model(all_tasks):
    model = lp_cpp.CriticalSectionsOfTaskset()

    for t in all_tasks:
        cs = model.new_task()
        for (id, len, outer) in t.critical_sections.all_flat():
            cs.add(id, len, outer)

    return model

def apply_lp_dflp_bounds(all_tasks, resource_mapping):
    model = get_cpp_model(all_tasks)
    topo  = get_cpp_topology(resource_mapping)
    res = lp_cpp.lp_dflp_bounds(model, topo)
    for i, t in enumerate(all_tasks):
        t.suspended = res.get_remote_blocking(i)
        t.blocked   = res.get_blocking_term(i)
    return res

def apply_lp_dpcp_bounds(all_tasks, resource_mapping, use_rta = True):
    model = get_cpp_model(all_tasks)
    topo  = get_cpp_topology(resource_mapping)
    res = lp_cpp.lp_dpcp_bounds(model, topo, use_rta)
    for i, t in enumerate(all_tasks):
        t.suspended = res.get_remote_blocking(i)
        t.blocked   = res.get_blocking_term(i)
    return res


def apply_lp_mpcp_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_mpcp_bounds(model)

    for i,t in enumerate(all_tasks):
        # remote blocking <=> self-suspension time
        t.suspended = res.get_remote_blocking(i)
        # all blocking, including local blocking
        t.blocked   = res.get_blocking_term(i)
        t.locally_blocked = res.get_local_blocking(i)

    return res

def apply_lp_part_fmlp_bounds(all_tasks):
    # LP-based analysis of the partitioned, preemptive FMLP+
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_part_fmlp_bounds(model)

    for i,t in enumerate(all_tasks):
        # remote blocking <=> self-suspension time
        t.suspended = res.get_remote_blocking(i)
        # all blocking, including local blocking
        t.blocked   = res.get_blocking_term(i)
        t.locally_blocked = res.get_local_blocking(i)

    return res

def apply_generalized_fmlp_bounds(all_tasks, cluster_size, using_edf):
    # LP-based analysis of the generalized FMLP+
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_gfmlp_bounds(model, cluster_size, using_edf)

    if cluster_size == 1:
        # partitioned scheduling: charge uniproc-analysis compatible blocking
        for i,t in enumerate(all_tasks):
            # remote blocking <=> self-suspension time
            t.suspended = res.get_remote_blocking(i)
            # all blocking, including local blocking
            t.blocked   = res.get_blocking_term(i)
            t.locally_blocked = res.get_local_blocking(i)
    else:
        # global analysis => no special treatment of local blocking
        for i,t in enumerate(all_tasks):
            # remote blocking <=> self-suspension time
            t.suspended = res.get_blocking_term(i)
            # all blocking
            t.blocked   = res.get_blocking_term(i)

    return res


def apply_omip_bounds(all_tasks, num_cpus, procs_per_cluster):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_omip_bounds(model, num_cpus, procs_per_cluster)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_dummy_bounds(all_tasks):
    model = get_cpp_model(all_tasks, True)
    res = lp_cpp.dummy_bounds(model)
    for i,t in enumerate(all_tasks):
        t.blocked   = res.get_local_blocking(i)
        t.remote_blocking = res.get_remote_blocking(i)
    return res


def apply_msrp_bounds(all_tasks, num_cpus):
    for t in all_tasks:
        if t.partition == None:
            t.partition= num_cpus + 1
    model = get_cpp_model(all_tasks, True)
    res = cpp.msrp_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        t.blocked   = res.get_local_blocking(i)
        t.cost      += res.get_remote_blocking(i)
        t.remote_blocking = res.get_remote_blocking(i)

def apply_pfp_lp_preemptive_fifo_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_preemptive_fifo_spinlock_bounds(model)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_msrp_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_msrp_bounds(model)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_unordered_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_unordered_spinlock_bounds(model, False)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_preemptive_unordered_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_unordered_spinlock_bounds(model, True)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_prio_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_prio_spinlock_bounds(model, False)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_preemptive_prio_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_prio_spinlock_bounds(model, True)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_prio_fifo_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_prio_fifo_spinlock_bounds(model, False)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_preemptive_prio_fifo_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_prio_fifo_spinlock_bounds(model, True)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_pfp_lp_baseline_spinlock_bounds(all_tasks):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_pfp_baseline_spinlock_bounds(model)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def apply_omip_bounds(all_tasks, num_cpus, procs_per_cluster):
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_omip_bounds(model, num_cpus, procs_per_cluster)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_pip_bounds(all_tasks, num_cpus):
    # LP-based analysis of the PIP under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_global_pip_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_ppcp_bounds(all_tasks, num_cpus):
    # LP-based analysis of the PPCP under global scheduling
    model = get_cpp_model(all_tasks)
    reasonable = is_reasonable_priority_assignment(num_cpus, all_tasks)
    res = lp_cpp.lp_ppcp_bounds(model, num_cpus, reasonable)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_sa_gfmlp_bounds(all_tasks, num_cpus):
    # LP-based s-aware analysis of the FMLP under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_sa_gfmlp_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_global_fmlpp_bounds(all_tasks, num_cpus):
    # LP-based s-aware analysis of the FMLP+ under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_global_fmlpp_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_prsb_bounds(all_tasks, num_cpus):
    # LP-based s-aware analysis of the PRSB under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_prsb_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_no_progress_fifo_bounds(all_tasks, num_cpus):
    # LP-based s-aware analysis for no progress and fifo queuing under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_no_progress_fifo_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_no_progress_priority_bounds(all_tasks, num_cpus):
    # LP-based s-aware analysis for no progress and priority queuing under global scheduling
    model = get_cpp_model(all_tasks)
    res = lp_cpp.lp_no_progress_priority_bounds(model, num_cpus)

    for i,t in enumerate(all_tasks):
        # Note: this is a response-time bound, not just
        # blocking. Returns ULONG_MAX if LP could not be
        # solved.
        t.response_time = t.cost + res.get_blocking_term(i)
    return res

def apply_pfp_nested_fifo_spinlock_bounds(all_tasks):
    task_info    = get_cpp_model(all_tasks, no_requests=True)
    nested_model = get_cpp_nested_cs_model(all_tasks)
    res = lp_cpp.lp_nested_fifo_spinlock_bounds(task_info, nested_model)
    for i, _ in enumerate(all_tasks):
        all_tasks[i].blocked = res.get_blocking_term(i)
    return res

def pedf_msrp_is_schedulable(all_tasks):
    # LP-based schedulability analysis based processor-demand criterion (PDC) for MSRP
    model = get_cpp_model(all_tasks, use_task_deadline=True)
    return lp_cpp.lp_pedf_msrp_is_schedulable(model)

def pedf_fifo_preempt_is_schedulable(all_tasks):
    # LP-based schedulability analysis based processor-demand criterion (PDC) for FIFO preemptive spin locks
    model = get_cpp_model(all_tasks, use_task_deadline=True)
    return lp_cpp.lp_pedf_fifo_preempt_is_schedulable(model)

def pedf_lockfree_preempt_is_schedulable(all_tasks):
    # LP-based schedulability analysis based processor-demand criterion (PDC) for preemptive lock-free
    model = get_cpp_model(all_tasks, use_task_deadline=True)
    return lp_cpp.lp_pedf_lockfree_preempt_is_schedulable(model)

def pedf_lockfree_NP_is_schedulable(all_tasks):
    # LP-based schedulability analysis based processor-demand criterion (PDC) for NP lock-free
    model = get_cpp_model(all_tasks, use_task_deadline=True)
    return lp_cpp.lp_pedf_lockfree_NP_is_schedulable(model)

def pedf_msrp_classic_is_schedulable(all_tasks, num_cpus):
    # MSRP classic analysis based on QPA
    model = get_cpp_model(all_tasks, use_task_deadline=True)
    return cpp.pedf_msrp_classic_is_schedulable(model, num_cpus)


