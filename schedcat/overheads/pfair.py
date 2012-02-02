from __future__ import division

from .quanta import quantize_wcet, quantize_period, account_for_delayed_release, stagger_latency

def charge_scheduling_overheads(oheads, num_cpus, dedicated_irq, taskset,
                                staggered=False, total_cpus=None,
                                aligned_periodic_releases=False):
    if not oheads or not taskset:
        return taskset

    qlen   = oheads.quantum_length
    ev_lat = oheads.release_latency(taskset)
    rel_oh = oheads.release(taskset)

    # account for reduced effective quantum length
    qeff = qlen \
        - ev_lat \
        - oheads.tick(taskset) \
        - oheads.schedule(taskset) \
        - oheads.ctx_switch(taskset) \
        - oheads.cache_affinity_loss(taskset)

    if not dedicated_irq:
        # account for release interrupts
        qeff -= (len(taskset) - 1) * rel_oh

    # Is any useful time left in the quantum? With short quanta and high
    # overheads, this may not be the case (in the analyzed worst case).
    if qeff <= 0:
        return False

    # apply reduction
    taskset = quantize_wcet(qlen, taskset, qeff)
    if not taskset:
        return False

    # Account for release delay.
    if not aligned_periodic_releases:
        # Default sporadic mode: job releases are triggered sporadically,
        # but newly released jobs are not considered for scheduling until
        # the next quantum boundary.
        release_delay = qlen + ev_lat + rel_oh
    else:
        # "Polling" mode. Periodic job releases are triggered
        # at each quantum boundary without any delays.
        release_delay = 0

    # shortcut: we roll staggering into release delay
    if staggered:
        if total_cpus is None:
            total_cpus = num_cpus;
        release_delay += stagger_latency(total_cpus, qlen)

    taskset = account_for_delayed_release(release_delay, taskset)
    if not taskset:
        return False

    return quantize_period(qlen, taskset, deadline=True)
