#!/usr/bin/env python
#
# Copyright (c) 2007,2008,2009, Bjoern B. Brandenburg <bbb [at] cs.unc.edu>
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
Simple bin-packing heuristics in Python.

Based on:

    OPTIMIZATION THEORY
    by Hubertus Th. Jongen, Klaus Meer, Eberhard Triesch
    ISBN 1-4020-8099-9
    KLUWER ACADEMIC PUBLISHERS
    http://www.springerlink.com/content/h053786u87674865/

and:

   http://york.cuny.edu/~malk/tidbits/tidbit-bin-packing.html

"""

id = lambda x: x

class DidNotFit(Exception):
    def __init__(self, item):
        self.item = item

    def __str__(self):
        return '%s could not be packed' % str(self.item)

def ignore(_):
    pass

def report_failure(x):
    raise DidNotFit(x)

def value(sets, weight=id):
    return sum([sum([weight(x) for x in s]) for s in sets])

def next_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
             empty_bin=list):
    sets = [empty_bin() for _ in xrange(0, bins)]
    cur  = 0
    sum  = 0.0
    for x in items:
        c = weight(x)
        while sum + c > capacity:
            sum = 0.0
            cur += 1
            if cur == bins:
                misfit(x)
                return sets
        sets[cur] += [x]
        sum += c
    return sets

def first_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
              empty_bin=list):
    sets = [empty_bin() for _ in xrange(0, bins)]
    sums = [0.0 for _ in xrange(0, bins)]
    for x in items:
        c = weight(x)
        for i in xrange(0, bins):
            if sums[i] + c <= capacity:
                sets[i] += [x]
                sums[i] += c
                break
        else:
            misfit(x)

    return sets

def worst_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
              empty_bin=list):
    sets = [empty_bin() for _ in xrange(0, bins)]
    sums = [0.0 for _ in xrange(0, bins)]
    for x in items:
        c = weight(x)
        # pick the bin where the item will leave the most space
        # after placing it, aka the bin with the least sum
        candidates = [s for s in sums if s + c <= capacity]
        if candidates:
            # fits somewhere
            i = sums.index(min(candidates))
            sets[i] += [x]
            sums[i] += c
        else:
            misfit(x)
    return sets

def almost_worst_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
              empty_bin=list):
    sets = [empty_bin() for _ in xrange(0, bins)]
    sums = [0.0 for _ in xrange(0, bins)]
    for x in items:
        c = weight(x)
        # pick the bin where the item will leave almost the most space
        # after placing it, aka the bin with the second to least sum
        candidates = [s for s in sums if s + c <= capacity]
        if candidates:
            # fits somewhere
            candidates.sort()
            i = sums.index(candidates[1] if len(candidates) > 1 else candidates[0])
            sets[i] += [x]
            sums[i] += c
        else:
            misfit(x)
    return sets

def best_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
             empty_bin=list):
    sets = [empty_bin()  for _ in xrange(0, bins)]
    sums = [0.0 for _ in xrange(0, bins)]
    for x in items:
        c = weight(x)
        # find the first bin that is sufficiently large
        idxs = range(0, bins)
        idxs.sort(key=lambda i: sums[i], reverse=True)
        for i in idxs:
            if sums[i] + c <= capacity:
                sets[i] += [x]
                sums[i] += c
                break
        else:
            misfit(x)
    return sets

def any_fit(items, bins, capacity=1.0, weight=id, misfit=ignore,
             empty_bin=list):
    for h in [next_fit, first_fit, worst_fit, almost_worst_fit, best_fit]:
        try:
            sets = h(items, bins, capacity, weight, report_failure, empty_bin)
            return sets
        except DidNotFit as dnf:
            pass
    # if we get here, none of the heuristics worked
    misfit(dnf.item)
    # if we get here, misfit did not raise an exception => return something
    return next_fit(items, bins, capacity, weight, misfit, empty_bin)

def decreasing(algorithm):
    def alg_decreasing(items, bins, capacity=1.0, weight=id, *args, **kargs):
        # don't clobber original items
        items_sorted = list(items)
        items_sorted.sort(key=weight, reverse=True)
        return algorithm(items_sorted, bins, capacity, weight, *args, **kargs)
    return alg_decreasing

next_fit_decreasing  = decreasing(next_fit)
first_fit_decreasing = decreasing(first_fit)
worst_fit_decreasing = decreasing(worst_fit)
best_fit_decreasing  = decreasing(best_fit)
almost_worst_fit_decreasing = decreasing(almost_worst_fit)
any_fit_decreasing   = decreasing(any_fit)

