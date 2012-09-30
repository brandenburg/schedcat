from __future__ import division

from math import ceil, floor

from schedcat.model.tasks import SporadicTask, TaskSystem

def charge_scheduling_overheads(oheads, num_cpus, dedicated_irq, taskset):
    if not oheads or not taskset:
        return TaskSystem(taskset)

    event_latency = oheads.release_latency(taskset)

    # pseudo-task representing the tick interrupt
    tck = oheads.tick(taskset)
    if tck > 0:
        tick_isr = SporadicTask(tck, oheads.quantum_length)
        tick_isr.jitter = event_latency
        tick_tasks = [tick_isr]
    else:
        tick_tasks = []

    # pseudo-tasks representing release interrupts
    rel_cost   = oheads.release(taskset)
    if not dedicated_irq and rel_cost > 0:
        release_tasks = [SporadicTask(rel_cost, t.period) for t in taskset]
        for isr in release_tasks:
            isr.jitter = event_latency
    else:
        release_tasks = [] # releases don't impact tasks directly

    # account for initial release delay as jitter
    release_delay = event_latency + oheads.release(taskset)
    if dedicated_irq:
        release_delay += oheads.ipi_latency(taskset)

    for t in taskset:
        if not 'jitter' in t.__dict__:
            t.jitter = 0
        t.jitter += release_delay

    # account for scheduling cost and CPMD
    sched  = oheads.schedule(taskset)
    cxs    = oheads.ctx_switch(taskset)
    cpmd   = oheads.cache_affinity_loss(taskset)
    preemption = 2 * (sched + cxs) + cpmd
    for t in taskset:
        t.cost += preemption

    return TaskSystem(tick_tasks + release_tasks + taskset)

def quantize_params(taskset):
    """After applying overheads, use this function to make
        task parameters integral again."""

    for t in taskset:
        t.cost     = int(ceil(t.cost))
        t.period   = int(floor(t.period))
        t.deadline = int(floor(t.deadline))
        t.jitter   = int(ceil(t.jitter))
        if t.density() > 1:
            return False

    return taskset
