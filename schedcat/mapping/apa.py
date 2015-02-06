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

from schedcat.model.tasks import TaskSystem

import schedcat.sched.native as native
import schedcat.sched

qpa = native.QPATest(1)

def qpa_it_fits(partition):
    return qpa.is_schedulable(schedcat.sched.get_native_taskset(partition))

def sorted_by_decreasing_difficulty(tasks):
    # heuristic: smaller affinity => harder to assign
    #            less slack => harder to scheduler
    return sorted(tasks, key=lambda t: (len(t.affinity), 1.0 - t.density()))

def edf_first_fit_decreasing_difficulty(tasks, all_cores=None):

    if all_cores is None:
        all_cores = set()
        for t in tasks:
            all_cores |= t.affinity

    unassigned = set(tasks)
    assignments = {}

    for t in tasks:
        t.orig_affinity = t.affinity
        t.affinity = set(t.affinity)

    for core in all_cores:
        ts = TaskSystem()
        # assign candidates as long as possible
        for t in sorted_by_decreasing_difficulty(
                        (t for t in unassigned if core in t.affinity) ):
            ts.append(t)
            if qpa_it_fits(ts):
                # ok, placed
                unassigned.remove(t)
            else:
                # nope, pop it off
                ts.pop()

        # finish
        assignments[core] = ts
        for t in unassigned:
            try:
                t.affinity.remove(core)
            except KeyError:
                pass # might not have affinity to core, not a problem

    for t in tasks:
        t.affinity = t.orig_affinity

    return (unassigned, assignments)



def edf_worst_fit_decreasing_difficulty(tasks):
    assignments = defaultdict(TaskSystem)
    unassigned = set()

    for t in sorted_by_decreasing_difficulty(tasks):
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
        if not candidates:
            unassigned.add(t)
        else:
            candidates.sort()
            (_, best) = candidates[0]
            assignments[best].append(t)
            assert qpa_it_fits(assignments[best])

    return (unassigned, assignments)

