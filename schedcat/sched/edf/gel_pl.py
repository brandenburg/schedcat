"""This module computes tardiness bounds for G-EDF-like schedulers.  Currently
   G-EDF and G-FL (See Erickson and Anderson ECRTS'12) are supported.  This
   module works by analyzing compliant vectors (see Erickson, Devi, Baruah
   ECRTS'10), analyzing the involved piecewise linear functions (see Erickson,
   Guan, Baruah OPODIS'10).  Notation is used as in EA'12.

   The analysis is general enough to support other GEL schedulers trivially.
"""

from __future__ import division

from math import ceil

from fractions import Fraction

from schedcat.util.quantor import forall

import schedcat.sched
if schedcat.sched.using_native:
    import schedcat.sched.native as native

import heapq

class AnalysisDetails:
    def __init__(self, tasks, gel_obj = None):
        num_tasks = len(tasks)
        self.tasks = tasks
        if gel_obj is not None:
            self.bounds = [gel_obj.get_bound(i) for i in range(num_tasks)]
            self.S_i = [gel_obj.get_Si(i) for i in range(num_tasks)]
            self.G_i = [gel_obj.get_Gi(i) for i in range(num_tasks)]

    def max_lateness(self):
        return max([self.bounds[i] - self.tasks[i].deadline
                    for i in range(len(self.tasks))])

def compute_gfl_response_details(no_cpus, tasks, rounds):
    """This function computes response time bounds for the given set of tasks
       using G-FL.  "rounds" determines the number of rounds of binary search;
       using "0" results in using the exact algorithm.
    """
    for task in tasks:
        task.pp = task.deadline - int((no_cpus - 1) / (no_cpus) * task.cost)
    return compute_response_details(no_cpus, tasks, rounds)

def compute_gedf_response_details(no_cpus, tasks, rounds):
    """This function computes response time bounds for a given set of tasks
       using G-EDF.  "rounds" determines the number of rounds of binary search;
       using "0" results in using the exact algorithm.
    """
    for task in tasks:
        task.pp = task.deadline
    return compute_response_details(no_cpus, tasks, rounds)

def compute_response_details(no_cpus, tasks, rounds):
    if (no_cpus == 1) and forall(tasks)(lambda t: t.pp == t.period):
        details = AnalysisDetails(tasks)
        details.bounds = [task.period for task in tasks]
        details.S_i = [0.0 for task in tasks]
        details.G_i = [0.0 for task in tasks]
        return details
    if schedcat.sched.using_native:
        native_ts = schedcat.sched.get_native_taskset(tasks)
        gel_obj = native.GELPl(no_cpus, native_ts, rounds)
        return AnalysisDetails(tasks, gel_obj)
    else:
        return compute_response_bounds(no_cpus, tasks, rounds)

def compute_response_bounds(no_cpus, tasks, rounds):
    """This function computes response time bounds for the given set of tasks
       and priority points.  "rounds" determines the number of rounds of binary
       search; using "0" results in using the exact algorithm.
    """

    if not has_bounded_tardiness(no_cpus, tasks):
        return None

    # Compute utilization ceiling
    util_ceil = int(ceil(tasks.utilization_q()))
    # First uniformly reduce scheduler priority points to derive analysis
    # priority points.  Due to uniform reduction, does not change scheduling
    # decisions.  Shown in EA'12 to improve bounds.
    sched_pps = [task.pp for task in tasks]
    min_priority_point = min(sched_pps)
    analysis_pps = [sched_pps[i] - min_priority_point
                    for i in range(len(sched_pps))]

    #Initialize S_i and Y_ints
    S_i = [Fraction(0)]*len(tasks)
    Y_ints = [Fraction(0)]*len(tasks)
    utilizations = [Fraction(task.cost, task.period) for task in tasks];
    # Calculate S_i and y-intercept (for G(\vec{x}) contributors)
    for i, task in enumerate(tasks):
        S_i[i] = max(Fraction(0), Fraction(task.cost) * (Fraction(1)
                     - Fraction(analysis_pps[i], task.period)))
        Y_ints[i] = (Fraction(0) - Fraction(task.cost, no_cpus)) * \
                     utilizations[i] + Fraction(task.cost) - S_i[i]

    if rounds == 0:
        s = compute_exact_s(no_cpus, tasks, util_ceil, sum(S_i), Y_ints,
                            utilizations)
    else:
        s = compute_binsearch_s(no_cpus, tasks, util_ceil, sum(S_i), Y_ints,
                                utilizations, rounds)

    details = AnalysisDetails(tasks)
    details.bounds = [int(ceil(s - Fraction(tasks[i].cost, no_cpus) + 
                      Fraction(tasks[i].cost) + Fraction(analysis_pps[i])))
                      for i in range(len(tasks))]
    details.S_i = S_i
    details.G_i = [Y_ints[i] + s * utilizations[i]
                   for i in range(len(tasks))]
    return details

def compute_exact_s(no_cpus, tasks, util_ceil, S, Y_ints, utilizations):
    replacements = []
    for i, task1 in enumerate(tasks):
        for j in range(i+1, len(tasks)):
            task2 = tasks[j]
            # Parallel lines do not intersect, and choice of identical lines
            # doesn't matter.  Ignore all such cases.
            if utilizations[i] != utilizations[j]:
                intersect = Fraction(Y_ints[j] - Y_ints[i],
                                     utilizations[i] - utilizations[j])
                if intersect >= 0:
                    if utilizations[i] < utilizations[j]:
                        replacements.append( (intersect, i, j) )
                    else:
                        replacements.append( (intersect, j, i) )
    # Break ties by order of increasing utilization of replaced tasks.  Avoids
    # an edge case.  Consider tasks A, B, C, in order of decreasing
    # utilization.  If the top m-1 items include tasks B and C, it is possible
    # (without the tie break) to have the following replacements:
    # C->B (no change)
    # B->A (now A and C in set considered)
    # C->A (no change)
    #
    # However, proper behavior should include A and B in considered set.  The
    # tie break avoids this case.
    replacements.sort(key=lambda r: (r[0], utilizations[r[1]]))

    # The piecewise linear function we are tracing is G(s) + S(\tau) - ms, as
    # discussed (with L(s) in place of G(s)) in EGB'10.  We start at s = 0 and
    # trace its value each time a slope changes, hoping for a value of zero.

    # While tracing piecewise linear function, keep track of whether each task
    # contributes to G(\vec{x}).  Array implementation allows O(1) lookups and
    # changes.
    task_pres = [False]*len(tasks)
    
    # Initial value and slope.
    current_value = S
    current_slope = Fraction(-1 * no_cpus)

    init_pairs = heapq.nlargest(util_ceil - 1, enumerate(Y_ints),
                                key=lambda p: p[1])

    # For s = 0, just use Y-intercepts to determine what is present.
    for pair in init_pairs:
        task_pres[pair[0]] = True
        current_value += pair[1]
        current_slope += utilizations[pair[0]]

    # Index of the next replacement
    rindex = 0
    next_s = Fraction(0)
    zero = next_s + Fraction(1)
    while zero > next_s:
        current_s = next_s
        zero = current_s - Fraction(current_value, current_slope)
        if rindex < len(replacements):
            replacement = replacements[rindex]
            next_s = replacement[0]
            current_value += (next_s - current_s) * current_slope
            # Apply replacement, if appropriate.
            if task_pres[replacement[1]] and not task_pres[replacement[2]]:
                task_pres[replacement[1]] = False
                current_slope -= utilizations[replacement[1]]
                task_pres[replacement[2]] = True
                current_slope += utilizations[replacement[2]]
            rindex += 1
        else:
            next_s = zero + 1
    return zero

def compute_binsearch_s(no_cpus, tasks, util_ceil, S, Y_ints, utilizations,
                        rounds):
    def M(s):
        Gvals = heapq.nlargest(util_ceil - 1,
                               [Y_ints[i] + utilizations[i] * s
                                for i in range(len(tasks))])
        return sum(Gvals) + S - Fraction(no_cpus) * s

    min_s = Fraction(0)
    max_s = Fraction(1)
    while M(max_s) > 0:
        min_s = max_s
        max_s *= Fraction(2)

    for i in range(rounds):
        middle = Fraction(max_s + min_s, 2)
        if M(middle) < 0:
            max_s = middle
        else:
            min_s = middle

    #max_s is guaranteed to be a legal bound.
    return max_s

def has_bounded_tardiness(no_cpus, tasks):
    return tasks.utilization() <= no_cpus and \
        forall(tasks)(lambda t: t.period >= t.cost)

def bound_gedf_response_times(no_cpus, tasks, rounds):
    response_details = compute_gedf_response_details(no_cpus, tasks, rounds)

    return bound_response_times(tasks, response_details)

def bound_gfl_response_times(no_cpus, tasks, rounds):
    response_details = compute_gfl_response_details(no_cpus, tasks, rounds)

    return bound_response_times(tasks, response_details)

def bound_response_times(tasks, response_details):
    if response_details is None:
        return False

    else:
        for i in range(len(tasks)):
            tasks[i].response_time = response_details.bounds[i]
        return True

