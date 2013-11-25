#Necessary includes and stuff

from schedcat.locking.bounds import apply_task_fair_mutex_bounds, \
                                    assign_prio_pt_locking_prios

from schedcat.overheads.jlfp import charge_scheduling_overheads, \
                                    quantize_params

from schedcat.sched.edf.gel_pl import \
    bound_gfl_response_times, has_bounded_tardiness

from schedcat.overheads.locking import charge_spinlock_overheads

def copy_ts(ts, clusts):
    new_ts = []
    new_clusts = []
    for clust in clusts:
        new_clust = clust.copy()
        new_clust.cpus = clust.cpus
        new_clusts.append(new_clust)
        new_ts += new_clust
    return (new_ts, new_clusts)

def preprocess_ts(taskset, clusts, oheads):
    for clust in clusts:
        charge_spinlock_overheads(oheads, clust)
        for task in clust:
            #Initially assume completion by deadline and use G-FL
            task.response_time = task.deadline
            task.prio_pt = task.deadline - \
                           (clust.cpus - 1) / (clust.cpus) * task.cost
    assign_prio_pt_locking_prios(taskset)

def post_blocking_term_oh_inflation(oheads, clusts):
    for clust in clusts:
        inflation = oheads.syscall_in(len(clust))
        for t in clust:
            if t.arrival_blocked:
                t.cost += inflation
                t.arrival_blocked += inflation
        if not charge_scheduling_overheads(oheads, clust.cpus,
                                           True, clust):
            return False
        quantize_params(clust)
    return True

def bound_cfl_with_locks(tasks, clusts, oheads, cluster_size):
    preprocess_ts(tasks, clusts, oheads)
    completion_ok = False
    count = 0
    while not completion_ok:
        completion_ok = True
        new_ts, new_clusts = copy_ts(tasks, clusts)
        count += 1
        if count > 100:
            return False
        apply_task_fair_mutex_bounds(new_ts, cluster_size, 0)
        if not post_blocking_term_oh_inflation(oheads,
                                               new_clusts):
            return False
        for i, clust in enumerate(new_clusts):
            if not has_bounded_tardiness(clust.cpus, clust):
                return False
            bound_gfl_response_times(clust.cpus, clust, 15)
            for j, t in enumerate(clust):
                if t.response_time > clusts[i][j].response_time:
                    completion_ok = False
    return new_clusts
