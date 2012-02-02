from __future__ import division

from math import ceil

# All overhead accounting in this file assumes absence of any interrupts.

def charge_spinlock_overheads(oheads, tasks):
    if oheads is None or not tasks:
        return tasks

    # the individual charges
    rcost  = oheads.read_lock(tasks) + oheads.read_unlock(tasks)
    wcost  = oheads.lock(tasks) + oheads.unlock(tasks)
    scost = oheads.syscall_in(tasks) + oheads.syscall_out(tasks)

    # inflate each request and each task's exec cost
    for t in tasks:
        extra_wcet = 0

        for res_id in t.resmodel:
            req = t.resmodel[res_id]
            if req.max_reads:
                req.max_read_length += rcost
                req.max_read_length = int(ceil(req.max_read_length))
                extra_wcet += req.max_reads * rcost

            if req.max_writes:
                req.max_write_length += wcost
                req.max_write_length = int(ceil(req.max_write_length))
                extra_wcet += req.max_writes * wcost
            extra_wcet += req.max_requests * scost

        t.cost    += int(ceil(extra_wcet))
        if t.density() > 1:
            return False
    return tasks

# for shared-memory semaphore protocols such as MPCP, FMLP, OMLP, etc.
def charge_semaphore_overheads(oheads, preemptive, suspension_aware, tasks):
    if oheads is None or not tasks:
        return tasks

    lock   = oheads.lock(tasks)
    unlock = oheads.unlock(tasks)
    sysin  = oheads.syscall_in(tasks)
    sysout = oheads.syscall_out(tasks)
    sched  = oheads.schedule(tasks) + oheads.ctx_switch(tasks)
    cpmd   = oheads.cache_affinity_loss(tasks)
    ipi    = oheads.ipi_latency(tasks)

    # per-request execution cost increase (equ 7.3)
    # 3 sched: wait + resume + yield
    exec_increase = 3 * sched + \
        2 * sysin  + \
        2 * sysout + \
        1 *   lock + \
        1 * unlock + \
        2 * cpmd

    # delay to be woken up
    if suspension_aware:
        susp_increase = ipi
    else:
        # s-oblivious: count IPI as execution time
        susp_increase = 0
        exec_increase += ipi

    # For non-preemptive protocols, this is the remote case.
    # Additional local costs are charged separately.
    # This only affects the FMLP+, the partitioned OMLP, and the
    # clustered OMLP.
    cs_increase = ipi + sched + sysout + sysin + unlock

    # preemptive protocols, add in additional scheduling cost
    if preemptive:
        cs_increase += sched
    else:
        # non-preemptive semaphore: add additional delay to local cost
        cs_increase_local = cs_increase + sched

    # inflate each request and each task's exec cost
    for t in tasks:
        extra_wcet = 0
        extra_susp = 0

        for res_id in t.resmodel:
            req = t.resmodel[res_id]
            assert req.max_reads == 0 # doesn't handle RW at the moment

            if req.max_writes:
                if not preemptive:
                    req.max_write_length_local = int(ceil(req.max_write_length + cs_increase_local))
                req.max_write_length += cs_increase
                req.max_write_length = int(ceil(req.max_write_length))
                extra_wcet += req.max_writes * exec_increase
                extra_susp += req.max_writes * susp_increase

        t.cost    += int(ceil(extra_wcet))
        if suspension_aware:
            t.suspended += int(ceil(extra_susp))
        if t.density() > 1:
            return False
    return tasks

def charge_dpcp_overheads(oheads, tasks):
    if oheads is None or not tasks:
        return tasks

    lock   = oheads.lock(tasks)
    unlock = oheads.unlock(tasks)
    sysin  = oheads.syscall_in(tasks)
    sysout = oheads.syscall_out(tasks)
    sched  = oheads.schedule(tasks) + oheads.ctx_switch(tasks)
    cpmd   = oheads.cache_affinity_loss(tasks)
    ipi    = oheads.ipi_latency(tasks)


    exec_increase = sysin + sysout + 2 * sched + 2 * cpmd
    cs_increase   = 3 * sched + sysin + sysout + lock + unlock
    susp_increase = 2 * ipi   + cs_increase

    # inflate each request and each task's exec cost
    for t in tasks:
        extra_wcet = 0
        extra_susp = 0

        for res_id in t.resmodel:
            req = t.resmodel[res_id]
            assert req.max_reads == 0 # DPCP doesn't handle RW

            if req.max_writes:
                req.max_write_length += cs_increase
                req.max_write_length = int(ceil(req.max_write_length))
                extra_wcet += req.max_writes * exec_increase
                extra_susp += req.max_writes * susp_increase

        t.cost    += int(ceil(extra_wcet))
        t.suspended += int(ceil(extra_susp))
        if t.density() > 1:
            return False
    return tasks
