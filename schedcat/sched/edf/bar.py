"""G-EDF hard schedulability test

This module implements Sanjoy Baruah's G-EDF schedulability test as presented
in his paper "Techniques for Multiprocessor Global Schedulability Analysis."

The variable names are picked to resemble the paper and are not meant to be
understandable without the paper as a reference.
"""

from __future__ import division

from math      import floor, ceil
from itertools import izip
from schedcat.util.quantor   import forall
from schedcat.util.math      import topsum
from schedcat.util.iter      import imerge, uniq

def dbf(tsk, t):
    """Demand bound function for task tsk at time t."""
    if t <= 0:
        return 0
    return  max(0, (int(floor((t - tsk.deadline) / tsk.period)) + 1) * tsk.cost)

def brute_force_dbf_points_of_change(tsk, max_t, d_k):
    for t in xrange(0, max_t + 1):
        cur = dbf(tsk, t + d_k)
        lst = dbf(tsk, t - 1 + d_k)
        if cur != lst:
            yield t

def dbf_points_of_change(tsk, max_t):
    """Return iterator over t where dbf(tsk, t) changes."""
    yield 0
    t = tsk.deadline
    while t <= max_t:
        yield t
        t += tsk.period

def dbf_points_of_change_dk(tsk, max_t, dk):
    """Return iterator over t where dbf(tsk, t) changes."""
    for pt in dbf_points_of_change(tsk, max_t + dk):
        offset = pt - dk
        if offset >= 0:
            yield offset

def all_dbf_points_of_change_dk(all_tsks, max_t, dk):
    all_points = [dbf_points_of_change_dk(t, max_t, dk) for t in all_tsks]
    return uniq(imerge(lambda x,y: x < y, *all_points))

# The definition of I1() and I2() diverge from the one given in the
# RTSS'07 paper. According to S. Baruah: "The second term in the min --
# A_k+D_k-C_k -- implicitly assumes that the job missing its deadline
# executes for C_k time units, whereas it actually executes for strictly
# less than C_k. Hence this second term should be --A_k+D_k(-C_k -
# \epsilon); for task systems with integer parameters, epsilon can be
# taken to e equal to one. [...] A similar modification may need to be
# made for the definition of I2."

def I1(tsk_i, tsk_k, a_k):
    d_k = tsk_k.deadline
    c_k = tsk_k.cost
    if tsk_k == tsk_i:
        return min(dbf(tsk_i, a_k + d_k) - c_k, a_k)
    else:
        return min(dbf(tsk_i, a_k + d_k), a_k + d_k - (c_k - 1))

def dbf_(tsk, t):
    """dbf() for carry-in scenario"""
    return int(floor(t / tsk.period)) * tsk.cost + min(tsk.cost, t % tsk.period)

def I2(tsk_i, tsk_k, a_k):
    d_k = tsk_k.deadline
    c_k = tsk_k.cost
    if tsk_k == tsk_i:
        return min(dbf_(tsk_i, a_k + d_k) - c_k, a_k)
    else:
        return min(dbf_(tsk_i, a_k + d_k), a_k + d_k - (c_k - 1))

def Idiff(tsk_i, tsk_k, a_k):
    return I2(tsk_i, tsk_k, a_k) - I1(tsk_i, tsk_k, a_k)

def task_schedulable_for_offset(all_tsks, tsk_k, a_k, m):
    """Tests condition 8 from the paper"""
    I1s    = [I1(tsk_i, tsk_k, a_k) for tsk_i in all_tsks]
    Idiffs = [I2(tsk_i, tsk_k, a_k) - i1 for (tsk_i, i1) in izip(all_tsks, I1s)]
    Idiff  = topsum(Idiffs, None, m -1)
    return sum(I1s) + Idiff <= m * (a_k + tsk_k.deadline - tsk_k.cost)

def ak_bounds(all_tsks, m):
    U = all_tsks.utilization()
    c_sigma = topsum(all_tsks, lambda t: t.cost, m - 1)
    y = sum([(t.period - t.deadline) * t.utilization() for t in all_tsks])
    mu      = m - U
    def ak_bound(tsk_k):
        # Equation 9 in the paper
        return (c_sigma - tsk_k.deadline * mu + y + m * tsk_k.cost) / mu
    return [ak_bound(t) for t in all_tsks]

def is_schedulable(m, tasks):
    """Are the given tasks schedulable on m processors?"""
    if tasks.utilization() >= m or not forall(tasks)(lambda t: t.constrained_deadline()):
        return False
    for (tsk_k, a_k_bound) in izip(tasks, ak_bounds(tasks, m)):
        for a_k in all_dbf_points_of_change_dk(tasks, a_k_bound, tsk_k.deadline):
            if not task_schedulable_for_offset(tasks, tsk_k, a_k, m):
                return False
    return True

