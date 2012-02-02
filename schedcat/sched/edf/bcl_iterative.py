"""
Implementation of  Marko Bertogna, Michele Cirinei, and Giuseppe Lipari
iterative schedulability test. This implementation follows the description in:

    Schedulability analysis of global scheduling algorithms on
    multiprocessor platforms by Marko Bertogna, Michele Cirinei, Giuseppe
    Lipari to appear in Journal IEEE Transactions on Parallel and
    Distributed Systems (2008).
"""

from __future__ import division

from math import floor, ceil

def interfering_jobs(length, ti):
    "Equ. (15) in the paper."
    return int(floor((length + ti.deadline - ti.cost - ti.bcl_slack) / ti.period))

def wk_interfering_workload(length, ti):
    "General work-conserving case, Equ. (14) in the paper."
    jobs = interfering_jobs(length, ti)
    return jobs * ti.cost + min(ti.cost, length + ti.deadline - ti.cost
                                - ti.bcl_slack - jobs * ti.period)

def edf_interfering_workload(length, ti):
    "Equ. (17) in the paper."
    jobs = int(floor(length / ti.period))
    return jobs * ti.cost + min(ti.cost,
                                max(0, length - ti.bcl_slack - jobs * ti.period))

def edf_slack_update(tk, tasks, no_cpus):
    """Compute slack in the case of G-EDF.
       Equ. (18) in the paper.
    """
    other_work = 0
    for ti in tasks:
        if tk != ti:
            other_work += min(edf_interfering_workload(tk.deadline, ti),
                               # the '+ 1' below assumes integral time
                              tk.deadline - tk.cost + 1)
    return tk.deadline - tk.cost - int(floor(other_work / no_cpus))

def is_schedulable(no_cpus, tasks, round_limit=None):
    """"Iteratively improve slack bound for each task until either the system
        is deemed to be feasible, no more improvements could be found, or
        the round limit (if given) is reached.
    """
    for t in tasks:
        t.bcl_slack = 0.0
    updated  = True
    feasible = False
    round    = 0
    while updated and not feasible and (not round_limit or round < round_limit):
        round += 1
        feasible = True
        updated  = False
        for tk in tasks:
            new_bound    = edf_slack_update(tk, tasks, no_cpus)
            feasible     = feasible and new_bound >= 0
            updated      = updated or new_bound > tk.bcl_slack
            tk.bcl_slack = max(tk.bcl_slack, new_bound)
    return feasible
