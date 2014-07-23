from __future__ import division

from math import ceil, floor
import heapq

def charge_initial_load(oheads, taskset):
    """Increase WCET to reflect the cost of establishing a warm cache.
    Note: assumes that .wss (working set size) has been populated in each task.
    """
    if oheads:
        for ti in taskset:
            load = oheads.initial_cache_load(ti.wss)
            assert load >= 0 # negative overheads make no sense
            ti.cost += load
            if ti.density() > 1:
                # infeasible
                return False
    return taskset

def preemption_centric_irq_costs(oheads, dedicated_irq, taskset):
    n      = len(taskset)
    n_rel_irq = 0
    qlen   = oheads.quantum_length
    tck    = oheads.tick(n)
    ev_lat = oheads.release_latency(n)

    # tick interrupt
    utick = tck / qlen

    urel  = 0.0
    if not dedicated_irq:
        rel   = oheads.release(n)
        for ti in taskset:
            if not hasattr(ti, 'early_releasing') or ti.early_releasing == False:
                urel += (rel / ti.period)
                n_rel_irq += 1

    # cost of preemption
    cpre_numerator = tck + ev_lat * utick
    if not dedicated_irq:
        cpre_numerator += n_rel_irq * rel + ev_lat * urel

    uscale = 1.0 - utick - urel

    return (uscale, cpre_numerator / uscale)

def charge_scheduling_overheads(oheads, num_cpus, dedicated_irq, taskset):
    if not oheads:
        return taskset

    uscale, cpre = preemption_centric_irq_costs(oheads, dedicated_irq, taskset)

    if uscale <= 0:
        # interrupt overload
        return False

    n   = len(taskset)
    
    cpmd = [(ti, oheads.cache_affinity_loss(ti.wss)) \
                for ti in heapq.nlargest(2, taskset, lambda x: x.wss)]
    sched = 2 * (oheads.schedule(n) + oheads.ctx_switch(n))

    irq_latency = oheads.release_latency(n)

    if dedicated_irq:
        unscaled = 2 * cpre + oheads.ipi_latency(n) + oheads.release(n)
    elif num_cpus > 1:
        unscaled = 2 * cpre + oheads.ipi_latency(n)
    else:
        unscaled = 2 * cpre

    for ti in taskset:
        tasksched = sched
        if cpmd:
            if ti != cpmd[0][0]:
                tasksched += cpmd[0][1]
            elif len(cpmd) > 1:
                tasksched += cpmd[1][1]

        ti.period   -= irq_latency
        ti.deadline -= irq_latency
        ti.cost      = ((ti.cost + tasksched) / uscale) + unscaled
        if ti.density() > 1:
            return False

    return taskset

def quantize_params(taskset):
    """After applying overheads, use this function to make
        task parameters integral again."""

    for t in taskset:
        t.cost     = int(ceil(t.cost))
        t.period   = int(floor(t.period))
        t.deadline = int(floor(t.deadline))
        if not min(t.period, t.deadline) or t.density() > 1:
            return False

    return taskset
