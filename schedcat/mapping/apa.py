#!/usr/bin/env python
#
# Copyright (c) 2015 Bjoern B. Brandenburg <bbb [at] mpi-sws.org>
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

from collections import defaultdict

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
