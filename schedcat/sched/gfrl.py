"""This module computes tardiness bounds for Global Fair Relative Lateness
   (G-FRL).  schedulers.  Notation is used as in EA'12.
"""

from __future__ import division

from math import ceil

from fractions import Fraction

from schedcat.util.quantor import forall

import schedcat.sched
if schedcat.sched.using_native:
    import schedcat.sched.native as native

import schedcat.sched.edf.gel_pl as gel_pl

import heapq

LITTLE_S_TYPE = 1
BIG_S_TYPE = 0
BIG_G_TYPE = 2
DUMMY_TYPE = 3

class SlopeChange:
    def __init__(self, location, kind, index1, index2, slope1,
                 slope2):
        self.location = location
        self.kind = kind
        self.index1 = index1
        self.index2 = index2
        self.slope1 = slope1
        self.slope2 = slope2

def s_val(L, tasks, no_cpus):
    return min([L * Fraction(task.deadline) -
                Fraction((no_cpus - 1) * task.cost, no_cpus) for task in tasks])

def s_slope(L, tasks, no_cpus):
    tup = min([(L * Fraction(task.deadline) - Fraction((no_cpus - 1) *
                task.cost, no_cpus), task.deadline) for task in tasks])
    return tup[1]

def Si_val(i, L, tasks, no_cpus):
    task = tasks[i]
    raw_val = Fraction(task.cost) - Fraction(task.cost, task.period) * \
              (L * Fraction(task.deadline) - Fraction((no_cpus -1) * task.cost,
              no_cpus) - s_val(L, tasks, no_cpus))
    if (raw_val < 0):
        return Fraction(0)
    else:
        return raw_val

def Si_slope(i, L, tasks, no_cpus):
    task = tasks[i]
    raw_slope = Fraction(task.cost, task.period) * \
                (Fraction(-1 * task.deadline) + s_slope(L, tasks, no_cpus))
    raw_val = Fraction(task.cost) - Fraction(task.cost, task.period) * \
              (L * Fraction(task.deadline) - Fraction((no_cpus -1) * task.cost,
              no_cpus) - s_val(L, tasks, no_cpus))
    if (raw_val < 0) or ((raw_val == Fraction(0)) and (raw_slope < 0)):
        return Fraction(0)
    else:
        return raw_slope

def S_val(L, tasks, no_cpus):
    return sum([Si_val(i, L, tasks, no_cpus) for i in range(len(tasks))])

def S_slope(i, L, tasks, no_cpus):
    return sum([Si_slope(i, L, tasks, no_cpus) for i in range(len(tasks))])

def Gi_val(i, L, tasks, no_cpus):
    task = tasks[i]
    return (s_val(L, tasks, no_cpus) - Fraction(task.cost, no_cpus)) * \
           Fraction(task.cost, task.period) + Fraction(task.cost) - \
           Si_val(i, L, tasks, no_cpus)

def Gi_slope(i, L, tasks, no_cpus):
    task = tasks[i]
    return Fraction(task.cost, task.period) * s_slope(L, tasks, no_cpus) - \
           Si_slope(i, L, tasks, no_cpus)

def G_val(L, tasks, no_cpus):
    util_cap = int(ceil(tasks.utilization_q()))
    return sum(heapq.nlargest(util_cap - 1, [Gi_val(i, L, tasks, no_cpus)
                                            for i in range(len(tasks))]))

def G_slope(L, tasks, no_cpus):
    util_cap = int(ceil(tasks.utilization_q()))
    largest = heapq.nlargest(util_cap - 1, [(Gi_val(i, L, tasks, no_cpus),
                                            Gi_slope(i, L, tasks, no_cpus))
                                            for i in range(len(tasks))])
    return sum([tup[1] for tup in largest])

def func_val(L, tasks, no_cpus):
    return G_val(L, tasks, no_cpus) + S_val(L, tasks, no_cpus) - \
           Fraction(no_cpus) * s_val(L, tasks, no_cpus)

def func_slope(L, tasks, no_cpus):
    return G_slope(L, tasks, no_cpus) + S_slope(L, task, no_cpus) - \
           Fraction(no_cpus) * s_slope(L, tasks, no_cpus)

def compute_response_bounds(no_cpus, tasks):
    """This function computes response time bounds for the given set of tasks
       and priority points.
    """

    if not has_bounded_tardiness(no_cpus, tasks):
        return None

    util_cap = int(ceil(tasks.utilization_q()))

    # Y-intercepts of potential "s" values with respect to L
    # s = min(LD_i - \frac{m-1}{m} C_i)
    s_intercepts = [Fraction(-1 * (no_cpus - 1) * task.cost, no_cpus)
                    for task in tasks]
    # Order by decreasing deadline, with ties broken in order of decreasing
    # cost.
    ordered_tuples = sorted([(task.deadline, task.cost, i)
                             for i, task in enumerate(tasks)],
                             reverse=True)
    ordered_tasks = [(tup[2], tasks[tup[2]]) for tup in ordered_tuples]
    #Filter out tasks with equal deadlines - only the first can be the minimum.
    filtered_tasks = [tup for i, tup in enumerate(ordered_tasks)
                      if (i == 0) or (ordered_tasks[i-1][1].deadline
                      != ordered_tasks[i][1].deadline)]

    # If all tasks have the same deadline, or only one task, there are no slope
    # changes for s with respect to L.
    s_slope_changes = []
    # The task that crosses the s axis.
    zero_task_index, zero_task = filtered_tasks[0]
    prev_task_index, prev_task = filtered_tasks[0]
    for curr_task_index, curr_task in filtered_tasks[1:]:
        next_intersect = Fraction(s_intercepts[prev_task_index] -
                                  s_intercepts[curr_task_index],
                                  curr_task.deadline - prev_task.deadline)
        while s_slope_changes and \
              (s_slope_changes[-1].location >= next_intersect):
            prev_task_index = s_slope_changes[-1].index1
            prev_task = tasks[prev_task_index]
            s_slope_changes.pop()
            next_intersect = Fraction(s_intercepts[prev_task_index] -
                                      s_intercepts[curr_task_index],
                                      curr_task.deadline -
                                      prev_task.deadline)

        if next_intersect <= Fraction(0):
            zero_task_index, zero_task = (curr_task_index, curr_task)
        else:
            s_slope_changes.append(SlopeChange(next_intersect, LITTLE_S_TYPE,
                                   prev_task_index, curr_task_index,
                                   prev_task.deadline, curr_task.deadline))
        prev_task_index, prev_task = (curr_task_index, curr_task)

    # Slope of S_i evaluated at zero.  "r" for "raw" is before max with zero.
    S_i_r = [Fraction(0)]*len(tasks)
    S_i = [Fraction(0)]*len(tasks)
    # Slope of first derivative of S_i evaluated at zero.
    S_i_r_p = [Fraction(0)]*len(tasks)
    S_i_p = [Fraction(0)]*len(tasks)
    # Sum of S_i values
    S = Fraction(0)
    # Sum of slopes
    S_p = Fraction(0)
    G_vals = [Fraction(0)]*len(tasks)
    G_slopes = [Fraction(0)]*len(tasks)
    utilizations = [Fraction(task.cost, task.period) for task in tasks]
    S_slope_changes = []
    for i, task in enumerate(tasks):
        S_i_r[i] = Fraction(task.cost) - utilizations[i] * \
                 (s_intercepts[i] - s_intercepts[zero_task_index])
        S_i_r_p[i] = utilizations[i] * Fraction(-1 * task.deadline +
                                                zero_task.deadline)
        if (S_i_r[i] < 0) or ((S_i_r[i] == Fraction(0)) and (S_i_r_p[i] < 0)):
            S_i[i] = Fraction(0)
            S_i_p[i] = Fraction(0)
        else:
            S_i[i] = S_i_r[i]
            S_i_p[i] = S_i_r_p[i]
        S += S_i[i]
        S_p += S_i_p[i]
        G_vals[i] = (s_intercepts[zero_task_index] - Fraction(task.cost,
                     no_cpus)) * utilizations[i] + Fraction(task.cost) - S_i[i]
        G_slopes[i] = Fraction(zero_task.deadline) * utilizations[i] - S_i_p[i]
        rindex = 0
        next_L = Fraction(0)
        next_value = S_i_r[i]
        next_slope = S_i_r_p[i]
        while rindex < (len(s_slope_changes) + 1):
            current_L = next_L
            current_value = next_value
            current_slope = next_slope
            current_effective_slope = current_slope
            if rindex < len(s_slope_changes):
                change = s_slope_changes[rindex]
                next_L = change.location
                next_value = current_value + (next_L - current_L) * \
                    current_slope
                next_slope = current_slope + utilizations[i] * \
                     (change.slope2 - change.slope1)
                rindex += 1
            else:
                rindex += 1
                change = None
                next_L = None
                # These are just to trigger the right cases for zero crossings.
                if current_slope < 0:
                    next_value = -1
                else:
                    next_value = current_value
            if current_value < 0:
                if next_value > 0:
                    zero = current_L - Fraction(current_value, current_slope)
                    S_slope_changes.append(SlopeChange(zero, BIG_S_TYPE, i, 1,
                                                       Fraction(0),
                                                       current_slope))
                else:
                    current_effective_slope = Fraction(0)
            elif current_value > 0 and next_value < 0:
                zero = current_L - Fraction(current_value, current_slope)
                S_slope_changes.append(SlopeChange(zero, BIG_S_TYPE, i, 2,
                                                   current_slope, Fraction(0)))
                current_effective_slope = Fraction(0)
            elif current_value == 0 and current_slope < 0:
                current_effective_slope = Fraction(0)
            next_effective_slope = next_slope
            if next_value < 0:
                next_effective_slope = Fraction(0)
            elif next_value == 0 and next_slope < 0:
                next_effective_slope = Fraction(0)
            if (next_L is not None) and \
                    (current_effective_slope != next_effective_slope):
                S_slope_changes.append(SlopeChange(next_L, BIG_S_TYPE, i, 3,
                                                   current_effective_slope,
                                                   next_effective_slope))

    slope_changes = s_slope_changes + S_slope_changes
    slope_changes.sort(key=lambda c: (c.location, c.kind))

    # The piecewise linear function we are tracing is G(s(L)) + S(s(L)) -
    # m*s(L), as discussed (with L(s) in place of G(s)) in EGB'10.  We start
    # at s = 0 and trace its value each time a slope changes, hoping for a
    # value of zero.

    # While tracing piecewise linear function, keep track of whether each task
    # contributes to G(\vec{x}).  Array implementation allows O(1) lookups and
    # changes.
    task_pres = [False]*len(tasks)

    # Initial value and slope.
    #First, account for "S(s(L)) -m*s(L)" terms.
    current_value = Fraction(-1 * no_cpus) * s_intercepts[zero_task_index] + S
    current_slope = Fraction(-1 * no_cpus * zero_task.deadline) + S_p

    init_pairs = heapq.nlargest(util_cap - 1, enumerate(G_vals),
                                key=lambda p: p[1])

    # For L = 0, just use Y-intercepts to determine what is present.
    for pair in init_pairs:
        task_pres[pair[0]] = True
        current_value += pair[1]
        current_slope += G_slopes[pair[0]]

    # Index of the next replacement
    G_slope_changes = []
    gindex = 0
    rindex = 0
    # Initialize with a dummy change that doesn't do anything.
    next_change = SlopeChange(Fraction(0), DUMMY_TYPE, None, None, None, None)
    # TODO: ensure that zero can't be "None" when no changes left.
    zero = None
    while (zero is None) or (zero > next_change.location):
        current_change = next_change
        if gindex < len(G_slope_changes):
            next_change = G_slope_changes[gindex]
            gindex += 1
        elif rindex < len(slope_changes):
            next_change = slope_changes[rindex]
            rindex += 1
        else:
            next_change = None
        if (current_change.kind != BIG_G_TYPE) and \
                ((next_change is None) or
                 (current_change.location != next_change.location)):
            # Need to update G_slope_changes.
            assert(gindex == len(G_slope_changes))
            G_slope_changes = []
            gindex = 0
            for i, task1 in enumerate(tasks):
                for j in range(i+1, len(tasks)):
                    task2 = tasks[j]
                    # Parallel lines do not intersect, and choice of identical
                    # lines doesn't matter.  Ignore all such cases.
                    if G_slopes[i] != G_slopes[j]:
                        intersect = Fraction(G_vals[j] - G_vals[i],
                                             G_slopes[i] - G_slopes[j]) + \
                                             current_change.location
                        if (intersect >= current_change.location) and \
                                ((next_change is None) or
                                 (intersect < next_change.location)):
                            if G_slopes[i] < G_slopes[j]:
                                G_slope_changes.append(SlopeChange(intersect,
                                                       BIG_G_TYPE, i, j,
                                                       G_slopes[i],
                                                       G_slopes[j]))
                            else:
                                G_slope_changes.append(SlopeChange(intersect,
                                                       BIG_G_TYPE, j, i,
                                                       G_slopes[j],
                                                       G_slopes[i]))
            # Break ties by order of increasing slope for replaced tasks.
            # Avoids an edge case.  Consider tasks A, B, C, in order of
            # decreasing utilization.  If the top m-1 items include tasks B and
            # C, it is possible (without the tie break) to have the following
            # replacements:
            #
            # C->B (no change)
            # B->A (now A and C in set considered)
            # C->A (no change)
            #
            # However, proper behavior should include A and B in considered
            # set.  The tie break avoids this case.
            G_slope_changes.sort(key=lambda c: (c.location, c.slope1))
            if G_slope_changes:
                # If we consumed a change that is now still future.
                if next_change is not None:
                    rindex -= 1
                next_change = G_slope_changes[0]
                gindex = 1

        if current_slope != 0:
            zero = current_change.location - Fraction(current_value,
                                                      current_slope)
            if (zero < current_change.location):
                zero = None
        else:
            zero = None

        if rindex < len(slope_changes):
            change = slope_changes[rindex]
            next_L = change.location
        if next_change is not None:
            if current_change.location != next_change.location:
                delta = next_change.location - current_change.location
                current_value += delta * current_slope
                for i in range(len(tasks)):
                    G_vals[i] += delta * G_slopes[i]
            # Apply replacement, if appropriate.
            if (next_change.kind == BIG_G_TYPE) and \
                        (task_pres[next_change.index1] and
                        not task_pres[next_change.index2]):
                task_pres[next_change.index1] = False
                current_slope -= next_change.slope1
                task_pres[next_change.index2] = True
                current_slope += next_change.slope2
            elif next_change.kind == LITTLE_S_TYPE:
                # Account for both change to G(s(L)) and m*s(L), but not to
                # S(s(L))
                delta = next_change.slope2 - next_change.slope1
                current_slope += Fraction(-1 * no_cpus) * delta
                for i in range(len(tasks)):
                    increase = utilizations[i] * delta
                    G_slopes[i] += increase
                    if task_pres[i]:
                        current_slope += increase
            elif next_change.kind == BIG_S_TYPE:
                delta = next_change.slope2 - next_change.slope1
                current_slope += delta
                G_slopes[next_change.index1] -= delta
                if task_pres[next_change.index1]:
                    current_slope -= delta
        else:
            next_change = SlopeChange(zero + Fraction(1), DUMMY_TYPE, None,
                                      None, None, None)



    assert(func_val(zero, tasks, no_cpus) == Fraction(0))
    s = min([zero * Fraction(task.deadline) - Fraction((no_cpus - 1) *
             task.cost, no_cpus)
             for task in tasks])
    analysis_pps = [min(zero * Fraction(task.deadline) - s - Fraction((no_cpus
                    - 1) * task.cost, no_cpus), Fraction(task.period))
                    for task in tasks]
    for i, pp in enumerate(analysis_pps):
        tasks[i].gfrl_pp = pp
    details = gel_pl.compute_response_bounds(no_cpus, tasks, analysis_pps,
                                             15)
    return details.bounds

def has_bounded_tardiness(no_cpus, tasks):
    return tasks.utilization() <= no_cpus and \
        forall(tasks)(lambda t: t.period >= t.cost)


def bound_response_times(no_cpus, tasks):
    response_bounds = compute_response_bounds(no_cpus, tasks)
    if response_bounds is None:
        return False

    else:
        for i in range(len(tasks)):
            tasks[i].response_time = response_bounds[i]
        return True
