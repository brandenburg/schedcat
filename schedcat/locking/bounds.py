import schedcat.locking.native as cpp

# assumes mutex constraints
def get_cpp_model(all_tasks, use_task_period=False):
    rsi = cpp.ResourceSharingInfo(len(all_tasks))
    for t in all_tasks:
        rsi.add_task(t.period,
                     t.period if use_task_period else t.response_time,
                     t.partition,
                     t.locking_prio)
        for req in t.resmodel:
            req = t.resmodel[req]
            rsi.add_request_rw(req.res_id, req.max_requests, req.max_length, cpp.WRITE)
    return rsi

def get_cpp_model_rw(all_tasks, use_task_period=False):
    rsi = cpp.ResourceSharingInfo(len(all_tasks))
    for t in all_tasks:
        rsi.add_task(t.period,
                     t.period if use_task_period else t.response_time,
                     t.partition,
                     t.locking_prio)
        for req in t.resmodel:
            req = t.resmodel[req]
            if req.max_writes > 0:
                rsi.add_request_rw(req.res_id, req.max_writes, req.max_write_length, cpp.WRITE)
            if req.max_reads > 0:
                rsi.add_request_rw(req.res_id, req.max_reads, req.max_read_length, cpp.READ)
    return rsi

def assign_edf_locking_prios(all_tasks):
    all_deadlines = set([t.deadline for t in all_tasks])
    prio = {}
    for i, dl in enumerate(sorted(all_deadlines)):
        prio[int(dl)] = i

    for t in all_tasks:
        t.locking_prio = prio[int(t.deadline)]

def assign_fp_locking_prios(all_tasks):
    # prioritized in index order
    for i, t in enumerate(all_tasks):
        t.locking_prio = i

# S-aware bounds

def apply_mpcp_bounds(all_tasks, use_virtual_spin=False):
    model = get_cpp_model(all_tasks)
    res = cpp.mpcp_bounds(model, use_virtual_spin)

    if use_virtual_spin:
        for i,t in enumerate(all_tasks):
            # no suspension time
            t.suspended = 0
            # all blocking, including arrival blocking
            t.blocked = res.get_blocking_term(i)
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

# S-oblivious bounds

def apply_suspension_oblivious(all_tasks, res):
    for i,t in enumerate(all_tasks):
        # s-oblivious <=> no suspension
        t.suspended = 0
        # might be zero
        t.arrival_blocked = res.get_arrival_blocking(i)
        # all blocking, including arrival blocking
        t.blocked   = res.get_blocking_term(i)
        # s-oblivious: charge it as execution cost
        t.cost     += t.blocked

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

# spinlocks are charged similarly to s-oblivious analysis

def apply_task_fair_mutex_bounds(all_tasks, procs_per_cluster,
                                 dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model(all_tasks)
    res = cpp.task_fair_mutex_bounds(model, procs_per_cluster, dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_task_fair_rw_bounds(all_tasks, procs_per_cluster,
                              dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model_rw(all_tasks)
    # mutex equivalent model
    model_mtx = get_cpp_model(all_tasks)
    res = cpp.task_fair_rw_bounds(model, model_mtx, procs_per_cluster, dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res

def apply_phase_fair_rw_bounds(all_tasks, procs_per_cluster,
                                 dedicated_irq=cpp.NO_CPU):
    model = get_cpp_model_rw(all_tasks)
    res = cpp.phase_fair_rw_bounds(model, procs_per_cluster, dedicated_irq)
    apply_suspension_oblivious(all_tasks, res)
    return res



### S-aware LP-based analysis of distributed locking protocols

try:
    import schedcat.locking.linprog.native as lp_cpp

    lp_cpp_available = True
except ImportError:
    lp_cpp_available = False

import schedcat.locking.linprog.dflp as dflp
import schedcat.locking.linprog.dpcp as dpcp

def apply_py_lp_bounds(bounds, all_tasks, resource_mapping, *args):
    for t in all_tasks:
        lp = bounds.get_lp_for_task(resource_mapping,
                                    all_tasks, t, *args)

        lp.kill_non_positive_vars()
        solution = lp.solve()

        # total blocking
        t.blocked   = int(solution(lp.objective_function))
        t.suspended = int(solution(lp.remote_objective))

def apply_lp_dflp_bounds(all_tasks, resource_mapping,
                         use_py=False):
    if use_py or not lp_cpp_available:
        apply_py_lp_bounds(dflp, all_tasks, resource_mapping)
    else:
        model = get_cpp_model(all_tasks)
        topo  = get_cpp_topology(resource_mapping)
        res = lp_cpp.lp_dflp_bounds(model, topo)
        for i, t in enumerate(all_tasks):
            t.suspended = res.get_remote_blocking(i)
            t.blocked   = res.get_blocking_term(i)
        return res

def apply_lp_dpcp_bounds(all_tasks, resource_mapping,
                         use_rta = True, use_py=False):
    if use_py or not lp_cpp_available:
        apply_py_lp_bounds(dpcp, all_tasks, resource_mapping, use_rta)
    else:
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
