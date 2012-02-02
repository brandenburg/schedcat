"""Global EDF soft real-time test and tardiness bounds, based on Devi & Anderson's work.
"""

from __future__ import division

from math import ceil

from schedcat.util.quantor import forall

def tardiness_x(no_cpus, tasks):
    """This function computes the X part of Uma Devi's G-EDF tardiness bound, as
       given in Corollary 4.11 on page 109 of Uma's thesis..

       This function assumes full preemptivity.
    """
    if not tasks:
        return 0
    U = tasks.utilization()
    if no_cpus == 1:
        if U <= 1:
            return 0
        else:
            return None
    by_util = [t.utilization() for t in tasks]
    by_util.sort(reverse=True)
    by_cost = [t.cost for t in tasks]
    by_cost.sort(reverse=True)

    Lambda = int(ceil(U)) - 1
    emin   = by_cost[-1]

    reduced_capacity = no_cpus - sum(by_util[0:Lambda - 1])
    if reduced_capacity <= 0:
        # bad: tardiness is not bounded
        return None

    reduced_cost = max(0, sum(by_cost[0:Lambda]) - emin)
    return int(ceil(reduced_cost / reduced_capacity))

def np_tardiness_x(no_cpus, tasks):
    """This function computes the X part of Uma Devi's G-EDF tardiness bound, as
       given in Corollary 4.3 in Uma's thesis, page 110.
    """
    if not tasks:
        return 0
    U = tasks.utilization()
    # by_util is mu in Uma's theorem
    by_util = [t.utilization() for t in tasks]
    by_util.sort(reverse=True)
    # by_cost is epsilon in Uma's theorem
    by_cost = [t.cost for t in tasks]
    by_cost.sort(reverse=True)

    Lambda = int(ceil(U)) - 1
    emin   = by_cost[-1]

    reduced_capacity = no_cpus - sum(by_util[0:Lambda - 1])
    if reduced_capacity <= 0:
        # bad: tardiness is not bounded
        return None

    block_idx = no_cpus - Lambda - 1
    reduced_cost = sum(by_cost[0:Lambda]) + sum(by_cost[0:block_idx]) - emin
    return int(ceil(reduced_cost / reduced_capacity))

def task_tardiness_bound(no_cpus, tasks, preemptive=True):
    x = 0
    # first check if the bound formulas are valid
    if not has_bounded_tardiness(no_cpus, tasks):
        return None
    if no_cpus > 1:
        if preemptive:
            x = tardiness_x(no_cpus, tasks)
        else:
            x = np_tardiness_x(no_cpus, tasks)
    else:
        x = 0
    return x

def has_bounded_tardiness(no_cpus, tasks):
    return tasks.utilization() <= no_cpus and \
        forall(tasks)(lambda t: t.period >= t.cost)

def bound_response_times(no_cpus, tasks, preemptive=True):
    # DA's work applies to implicit-deadline tasks
    assert forall(tasks)(lambda t: t.implicit_deadline())

    x = task_tardiness_bound(no_cpus, tasks, preemptive)
    if x is None:
        return False
    else:
        for t in tasks:
            t.response_time = t.deadline + t.cost + x
        return True
