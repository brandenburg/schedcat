#!/usr/bin/env python
#
# Copyright (c) 2015, 2016 Bjoern B. Brandenburg <bbb [at] mpi-sws.org>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holder nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS  PROVIDED BY THE COPYRIGHT HOLDERS  AND CONTRIBUTORS "AS IS"
# AND ANY  EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED  TO, THE
# IMPLIED WARRANTIES  OF MERCHANTABILITY AND  FITNESS FOR A  PARTICULAR PURPOSE
# ARE  DISCLAIMED. IN NO  EVENT SHALL  THE COPYRIGHT  OWNER OR  CONTRIBUTORS BE
# LIABLE  FOR   ANY  DIRECT,  INDIRECT,  INCIDENTAL,   SPECIAL,  EXEMPLARY,  OR
# CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT   NOT  LIMITED  TO,  PROCUREMENT  OF
# SUBSTITUTE  GOODS OR SERVICES;  LOSS OF  USE, DATA,  OR PROFITS;  OR BUSINESS
# INTERRUPTION)  HOWEVER CAUSED  AND ON  ANY  THEORY OF  LIABILITY, WHETHER  IN
# CONTRACT,  STRICT  LIABILITY, OR  TORT  (INCLUDING  NEGLIGENCE OR  OTHERWISE)
# ARISING IN ANY  WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF  ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

"""
This module provides EDF-based partitioning and semi-partitioning heuristics
that are aware of arbitrary processor affinity (APA) constraints. This allows
each task to be restricted to an arbitrary subset of processors. The placement
heuristics will respect any such placement constraints.

The main heuristics are first-fit and worst-fit decreasing, optionally in
combination with the C=D semi-partitioning heuristic due to Burns et al. On top
of this foundation, several other heuristics and meta-heuristics are provided
that, together, are higly effective at placing implicit-deadline tasks. See the
following paper for further details and explanations:

    [RTSS'16] B. Brandenburg and M. Guel, "Global Scheduling Not Required:
              Simple, Near-Optimal Multiprocessor Real-Time Scheduling with
              Semi-Partitioned Reservations", Proceedings of the 37th IEEE
              Real-Time Systems Symposium (RTSS 2016), December 2016.
"""


from __future__ import division
from math import floor, ceil

from collections import defaultdict

import operator

from schedcat.model.tasks import TaskSystem, SporadicTask

import schedcat.sched.native as native
import schedcat.sched

qpa = native.QPATest(1)

def qpa_it_fits(partition):
    if partition.density() <= 1.0:
        return True
    else:
        return qpa.is_schedulable(schedcat.sched.get_native_taskset(partition))

def qpa_split_off_chunk(partition, task_to_split):
    ts = schedcat.sched.get_native_taskset(partition)
    max_wcet = native.qpa_get_max_C_equal_D_cost(
            ts, task_to_split.cost, task_to_split.period)
    return max_wcet

def sorted_by_decreasing_difficulty(tasks, effective_affinity=None):
    # heuristic: smaller affinity => harder to assign
    #            less slack => harder to scheduler
    if effective_affinity:
        return sorted(tasks, key=lambda t: (len(effective_affinity[t.id]),
                                            1.0 - t.density()))
    else:
        return sorted(tasks, key=lambda t: (len(t.affinity), 1.0 - t.density()))

def split_task_C_equal_D(t, wcet_part_one, split_callback):
    t1 = SporadicTask(wcet_part_one, t.period, deadline=wcet_part_one, id=t.id)
    t2 = SporadicTask(t.cost - wcet_part_one, t.period,
                      deadline=t.deadline - wcet_part_one, id=t.id)
    t2.affinity = set(t.affinity)
    if split_callback:
        split_callback(t, t1, t2)
    return (t1, t2)

def try_edf_assign_task_worst_fit(assignments, t):
    "add t to a partition"
    # find all cores on which it would fit
    candidates = []
    for core in t.affinity:
        ts = assignments[core]
        ts.append(t)
        if qpa_it_fits(ts):
            # ok, would fit
            candidates.append((ts.density(), core))
        # pop it off
        ts.pop()
    if candidates:
        (_, best) = min(candidates)
        assignments[best].append(t)
        # assert qpa_it_fits(assignments[best])
        return True
    else:
        return False

def try_edf_split_task_worst_fit(assignments, t, split_callback, threshold):
    for core in sorted(t.affinity,
                       key=lambda c: assignments[c].density()):
        # can we provision a chunk here?
        ts = assignments[core]
        wcet = qpa_split_off_chunk(ts, t)
        if wcet and wcet >= threshold:
            wcet = min(wcet, t.cost - threshold)
            assert wcet >= threshold
            (t1, t2) = split_task_C_equal_D(t, wcet, split_callback)
            ts.append(t1)
            return (core, t2)
    return False

def try_edf_split_task_max_chunk_fit(assignments, t, split_callback, threshold):
    max_chunk = (0, -1)
    # try all cores, find largest possible chunk
    for c in t.affinity:
        val = (qpa_split_off_chunk(assignments[c], t), c)
        max_chunk = max(max_chunk, val)

    wcet, core = max_chunk
    if wcet and wcet >= threshold:
        # can we provision a chunk here?
        ts = assignments[core]
        wcet = min(wcet, t.cost - threshold)
        assert wcet >= threshold
        (t1, t2) = split_task_C_equal_D(t, wcet, split_callback)
        ts.append(t1)
        return (core, t2)

    return False

def remove_core_from_affinities(unassigned, affinity, core):
    for t in unassigned:
        affinity[t.id].discard(core)

def edf_worst_fit_decreasing_difficulty(tasks, with_splits=False,
                                        pre_assigned=None,
                                        split_callback=None,
                                        min_chunk_size=0,
                                        max_chunk_split=False):
    "The main WFD (semi-)partitioning heuristic."
    affinity = {}
    for t in tasks:
        affinity[t.id] = set(t.affinity)

    assignments = defaultdict(TaskSystem)
    if not pre_assigned is None:
        for c in pre_assigned:
            for t in pre_assigned[c]:
                assignments[c].append(t)

    unassigned = set(tasks)
    try_again = True
    while unassigned and try_again:
        try_again = False
        for t in sorted_by_decreasing_difficulty(unassigned):
            placed = try_edf_assign_task_worst_fit(assignments, t)
            if placed:
                unassigned.remove(t)
            elif with_splits and t.cost >= 2 * min_chunk_size:
                # try to split it
                if max_chunk_split:
                    placed = try_edf_split_task_max_chunk_fit(assignments, t,
                                    split_callback, min_chunk_size)
                else:
                    placed = try_edf_split_task_worst_fit(assignments, t,
                                    split_callback, min_chunk_size)
                if placed:
                    (core, t2) = placed
                    # Was able to split, let's start over with t2 under
                    # consideration in addition to all other unsplit tasks.
                    unassigned.remove(t)
                    unassigned.add(t2)
                    try_again = True
                    break

    return (unassigned, assignments)

def edf_first_fit_decreasing_difficulty(tasks, all_cores=None,
                                        with_splits=False,
                                        pre_assigned=None,
                                        split_callback=None,
                                        min_chunk_size=0):
    "The main FFD (semi-)partitioning heuristic."
    if all_cores is None:
        all_cores = set()
        for t in tasks:
            all_cores |= t.affinity

    unassigned = set(tasks)

    assignments = defaultdict(TaskSystem)
    if not pre_assigned is None:
        for c in pre_assigned:
            for t in pre_assigned[c]:
                assignments[c].append(t)

    affinity = {}
    for t in tasks:
        affinity[t.id] = set(t.affinity)

    for core in all_cores:
        ts = assignments[core]
        # assign candidates as long as possible
        for t in sorted_by_decreasing_difficulty(
                        (t for t in unassigned if core in affinity[t.id]),
                        effective_affinity=affinity):
            ts.append(t)
            if qpa_it_fits(ts):
                # ok, placed
                unassigned.remove(t)
            elif with_splits and t.cost >= min_chunk_size * 2:
                # nope, try to split it
                ts.pop()
                wcet = qpa_split_off_chunk(ts, t)

                if wcet and wcet >= min_chunk_size:
                    wcet = min(wcet, t.cost - min_chunk_size)
                    assert wcet >= min_chunk_size
                    (t1, t2) = split_task_C_equal_D(t, wcet, split_callback)
                    unassigned.remove(t)
                    unassigned.add(t2)
                    ts.append(t1)

                # assert qpa_it_fits(ts)
                break
            else:
                # nope, pop it off
                ts.pop()

        # finish
        remove_core_from_affinities(unassigned, affinity, core)

    return (unassigned, assignments)

def count_slices(mapping):
    return reduce(operator.add, (len(ts) for ts in mapping.itervalues()), 0)

def double_wfd_split(*args, **kargs):
    "The heuristic called 2WFD-C=D in the RTSS'16 paper."
    kargs['max_chunk_split'] = False
    (unassigned, mapping) = edf_worst_fit_decreasing_difficulty(*args, **kargs)

    if not 'with_splits' in kargs or not kargs['with_splits']:
        return (unassigned, mapping)

    kargs['max_chunk_split'] = True
    (unassigned2, mapping2) = edf_worst_fit_decreasing_difficulty(*args, **kargs)

    if len(unassigned) < len(unassigned2):
        return (unassigned, mapping)
    elif len(unassigned) == len(unassigned2):
        if count_slices(mapping) < count_slices(mapping2):
            return (unassigned, mapping)
        else:
            return (unassigned2, mapping2)
    else:
        return (unassigned2, mapping2)

def edf_assign_wfd_wfd_split(taskset, min_chunk_size=0, pre_assign_small=False):
    "The heuristic called WWFD in the RTSS'16 paper."
    if min_chunk_size and pre_assign_small:
        too_small = [t for t in taskset if t.cost < min_chunk_size * 2]
        rest = set((t for t in taskset if t.cost >= min_chunk_size * 2))
        if too_small:
            (unassigned, mapping) = double_wfd_split(
                                        too_small, with_splits=False)
            (unassigned, mapping) = double_wfd_split(
                                        unassigned | rest, pre_assigned=mapping,
                                        with_splits=False)
        else:
            (unassigned, mapping) = double_wfd_split(
                                        taskset, with_splits=False)
    else:
        (unassigned, mapping) = double_wfd_split(
                                    taskset, with_splits=False)
    if unassigned:
        (unassigned, mapping) = double_wfd_split(
                                unassigned, with_splits=True,
                                pre_assigned=mapping,
                                min_chunk_size=min_chunk_size)

    return (unassigned, mapping)

def edf_assign_ffd_wfd_split(taskset, min_chunk_size=0, pre_assign_small=False):
    "The heuristic called FWFD in the RTSS'16 paper."
    if min_chunk_size and pre_assign_small:
        too_small = [t for t in taskset if t.cost < min_chunk_size * 2]
        rest = set((t for t in taskset if t.cost >= min_chunk_size * 2))
        if too_small:
            (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                        too_small, with_splits=False)
            (unassigned, mapping) = double_wfd_split(
                                        unassigned | rest, pre_assigned=mapping,
                                        with_splits=False)
        else:
            (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                        taskset, with_splits=False)
    else:
        (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                    taskset, with_splits=False)
    if unassigned:
        (unassigned, mapping) = double_wfd_split(
                                unassigned, with_splits=True,
                                pre_assigned=mapping,
                                min_chunk_size=min_chunk_size)

    return (unassigned, mapping)

def edf_assign_wfd_ffd_split(taskset, min_chunk_size=0, pre_assign_small=False):
    "The heuristic called WFFD in the RTSS'16 paper."
    if min_chunk_size and pre_assign_small:
        too_small = [t for t in taskset if t.cost < min_chunk_size * 2]
        rest = set((t for t in taskset if t.cost >= min_chunk_size * 2))
        if too_small:
            (unassigned, mapping) = double_wfd_split(
                                        too_small, with_splits=False)
            (unassigned, mapping) = double_wfd_split(
                                        unassigned | rest, pre_assigned=mapping,
                                        with_splits=False)
        else:
            (unassigned, mapping) = double_wfd_split(
                                        taskset, with_splits=False)
    else:
        (unassigned, mapping) = double_wfd_split(
                                    taskset, with_splits=False)
    if unassigned:
        (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                unassigned, with_splits=True,
                                pre_assigned=mapping,
                                min_chunk_size=min_chunk_size)

    return (unassigned, mapping)

def edf_assign_ffd_ffd_split(taskset, min_chunk_size=0, pre_assign_small=False):
    "The heuristic called FFFD in the RTSS'16 paper."
    if min_chunk_size and pre_assign_small:
        too_small = [t for t in taskset if t.cost < min_chunk_size * 2]
        rest = set((t for t in taskset if t.cost >= min_chunk_size * 2))
        if too_small:
            (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                        too_small, with_splits=False)
            (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                        unassigned | rest, pre_assigned=mapping,
                                        with_splits=False)
        else:
            (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                        taskset, with_splits=False)
    else:
        (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                    taskset, with_splits=False)
    if unassigned:
        (unassigned, mapping) = edf_first_fit_decreasing_difficulty(
                                unassigned, with_splits=True,
                                pre_assigned=mapping,
                                min_chunk_size=min_chunk_size)

    return (unassigned, mapping)

def meta_preassign_failures(preassign_heuristic, regular_heuristic, taskset):
    "The meta-heuristic called PAF in the RTSS'16 paper."
    # first try the regular heuristic
    (unassigned, mapping) = regular_heuristic(taskset)
    if not unassigned:
        return (unassigned, mapping)

    # ok, we have some failures
    all = set(taskset)
    # get the set of task IDs that we should pre-assign
    to_pa_idx = set((t.id for t in unassigned))

    while True:
        to_pa = set((t for t in all if t.id in to_pa_idx))
        rest = all - to_pa
        (unassigned, mapping) = preassign_heuristic(to_pa)
        # if we can't even pre-assign, we give up
        give_up = unassigned

        # try to fit the rest
        (unassigned, mapping) = regular_heuristic(rest | unassigned, pre_assigned=mapping)
        if not unassigned or give_up:
            # found one that works!
            return (unassigned, mapping)
        else:
            # nope, more problematic ones, try again
            for t in unassigned:
                to_pa_idx.add(t.id)

def is_feasible_pt(task, pt):
    return floor(task.period / pt) >= ceil(task.cost / pt)

def meta_reduce_periods(heuristic, taskset,
    candidate_periods=None, threshold=0, reduce_all=False):
    "The meta-heuristic called RP in the RTSS'16 paper."

    if not candidate_periods:
        # Caller really should provide some reasonable periods,
        # but we can try to come up with some defaults...
        candidate_periods = set()
        for t in taskset:
            for k in xrange(1, 11):
                candidate_periods.add(t.period / k)
        candidate_periods = sorted(candidate_periods)

    ts = taskset.copy()
    (unassigned, mapping) = heuristic(ts)

    if not unassigned:
        return (unassigned, mapping)

    if not reduce_all:
        failed = set((t.id for t in unassigned))

    def transform(limit):
        options = [p for p in candidate_periods if p < limit and p >= threshold]
        options.reverse()
        ts = taskset.copy()
        for t in ts:
            if t.period >= limit and (reduce_all or t.id in failed):
                transformed = False
                # try periods in the acceptable range
                for p in options:
                    pt = t.period // p
                    if t.period % p == 0 and is_feasible_pt(t, pt):
                        t.period_transform(pt)
                        transformed = True
                        break
                # try smallest period that works
                if not transformed:
                    for p in [p for p in candidate_periods if p >= limit]:
                        pt = t.period // p
                        if t.period % p == 0 and is_feasible_pt(t, pt):
                            t.period_transform(pt)
                            transformed = True
                            break
        return ts

    for limit in reversed([p for p in candidate_periods if p >= threshold]):
        ts = transform(limit)
        (unassigned, mapping) = heuristic(ts)
        if not unassigned:
            break
        elif not reduce_all:
            for t in unassigned:
                failed.add(t.id)

    return (unassigned, mapping)
