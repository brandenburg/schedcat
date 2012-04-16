"""
Implementation of Bertogna and Cirinei's response time analysis test.

    "Response-Time Analysis for Globally Scheduled Symmetric
     Multiprocessor Platforms"
    M. Bertogna and M. Cirinei,
    Proceedings of the 28th IEEE International Real-Time Systems Symposium,
    pages 149--160, 2007.

"""

from __future__ import division
from math import floor


def rta_interfering_workload(length, ti):
    "Equ. (4) and (8)"
    interval = length + ti.deadline - ti.cost - ti.rta_slack
    jobs = int(floor(interval / ti.period))
    return jobs * ti.cost + min(ti.cost, interval % ti.period)

def edf_interfering_workload(length, ti):
    "Equs. (5) and (9)"
    # implicit floor by integer division
    jobs = int(floor(length / ti.period))
    return jobs * ti.cost + \
        min(ti.cost, max(0, length % ti.period - ti.rta_slack))

def response_estimate(tk, tasks, no_cpus, response_time):
    cumulative_work = 0
    delay_limit = response_time - tk.cost + 1
    for ti in tasks:
        if ti != tk:
            cumulative_work += min(rta_interfering_workload(response_time, ti),
                                   edf_interfering_workload(tk.deadline, ti),
                                   delay_limit)
    return tk.cost + int(floor(cumulative_work / no_cpus))

def rta_fixpoint(tk, tasks, no_cpus, min_delta=None):
    """If the fixpoint search converges too slowly, then
    use min_delta to enforce a minimum step size."""
    # response time iteration, start with cost
    last, resp = tk.cost, response_estimate(tk, tasks, no_cpus, tk.cost)

    while last != resp and resp <= tk.deadline:
        if resp > last and resp - last < min_delta:
            resp = min(last + min_delta, tk.deadline)
        last, resp = resp, response_estimate(tk, tasks, no_cpus, resp)

    return resp

def is_schedulable(no_cpus, tasks, round_limit=25, min_fixpoint_step=0):
    """"Iteratively improve slack bound for each task until either the system
        is deemed to be feasible, no more improvements could be found, or
        the round limit (if given) is reached.
    """
    for t in tasks:
        t.rta_slack = 0
    updated     = True
    schedulable = False
    round       = 0

    while updated and not schedulable \
            and (not round_limit or round < round_limit):
        round += 1
        schedulable = True
        updated     = False
        for tk in tasks:
            # compute new response time bound
            response     = rta_fixpoint(tk, tasks, no_cpus,
                                        min_delta=min_fixpoint_step)
            if response <= tk.deadline:
                # this is a valid response time
                new_slack = tk.deadline - response
                if new_slack != tk.rta_slack:
                    tk.rta_slack = new_slack
                    updated = True
            else:
                # this one is currently not schedulable
                schedulable = False
    return schedulable

def bound_response_times(no_cpus, tasks, *args, **kargs):
    if is_schedulable(no_cpus, tasks, *args, **kargs):
        for t in tasks:
            t.response_time = t.deadline - t.rta_slack
        return True
    else:
        return False
