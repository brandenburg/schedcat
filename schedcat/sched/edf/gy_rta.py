"""
Efficient Response Time Analysis for EDF Scheduling.

This module implements RTA for EDF Scheduling as proposed by Nan Guan & Wang Yi
in their paper

"General and Efficient Response Time Analysis for EDF Scheduling", DATE 2014.
"""

from __future__ import division

from math import floor

from itertools import count

from heapq import merge

def sbf_uniprocessor(t):
    """
    Supply bound function of the system in time interval t for a uniprocessor.
    """
    return t

def sbf_inverse_uniprocessor(x):
    """
    Pseudo inverse of the supply bound function for a uniprocessor.
    """
    return x

def find_L_prime(tskset, sbf):
    """
    Returns L' at which rbf(L') <= sbf(L').
    """
    l_prime = 0
    while True:
        rbf_val = tskset.rbf(l_prime)
        sbf_val = sbf(l_prime)
        if rbf_val <= sbf_val:
            return l_prime
        l_prime += (rbf_val - sbf_val)

def find_L(tskset, sbf):
    """
    Returns L defined in the paper, L = L' + max(D_i)
    """
    return find_L_prime(tskset, sbf) + tskset.max_deadline()

def delta_values(tskset, sbf):
    """
    Yields valid values of delta as required by the proposed algorithms.
    """
    L = find_L(tskset, sbf)
    return tskset.dbf_points_of_change(max_t=L)

def approx_wcrt(tskset,
                sbf = sbf_uniprocessor,
                sbf_pseudo_inverse = sbf_inverse_uniprocessor):
    """
    Calculates worst-case response time by over-approximating slack bound
    of all tasks in tskset as per Algorithm 1 in the paper.
    NOTE: Sorts tskset using sort_by_deadline()
    """
    if tskset.utilization() > 1:
        return False # tskset is not EDF Schedulable
    tskset.sort_by_deadline()

    s = [float("inf")] * len(tskset)
    i = 0
    for delta in delta_values(tskset, sbf):
        try:
            if delta >= tskset[i+1].deadline:
                i += 1
        except IndexError:
            pass
        s[i] = min(s[i], delta - sbf_pseudo_inverse(tskset.dbf(delta)))
    for i in xrange(len(tskset)-1, -1, -1):
        tskset[i].response_time = tskset[i].deadline - s[i]
        if i == 0: break
        s[i-1] = min(s[i], s[i-1])

def mbf_i(tsk, delta, gamma):
    """
    Mixed bound function for task tsk with parameters delta and gamma.
    """
    return min(tsk.dbf(delta), tsk.rbf(gamma))

def mbf(tskset, delta, gamma):
    """
    Mixed bound function of the system with taskset tskset with parameters
    delta and gamma.
    """
    mixed = 0
    for tsk in tskset:
        mixed += mbf_i(tsk, delta, gamma)
    return mixed

def exact_wcrt(tskset,
               sbf = sbf_uniprocessor,
               sbf_pseudo_inverse = sbf_inverse_uniprocessor):
    """
    Calculates worst-case response time of all tasks in tskset by computing
    exact slack bound as per Algorithm 2 in the paper.
    NOTE: Sorts tskset using sort_by_deadline()
    """
    if tskset.utilization() > 1:
        return False # tskset is not EDF schedulable
    tskset.sort_by_deadline()

    s = [float("inf")] * len(tskset)
    i = 0
    for delta in delta_values(tskset, sbf):
        try:
            if delta >= tskset[i+1].deadline:
                i += 1
        except IndexError:
            pass
        if delta - sbf_pseudo_inverse(tskset.dbf(delta)) < s[i]:
            gamma_old = 0
            gamma_new = sbf_pseudo_inverse(mbf(tskset, delta, 0))
            while gamma_new != gamma_old:
                gamma_old = gamma_new
                gamma_new = sbf_pseudo_inverse(mbf(tskset, delta, gamma_old))
            s[i] = min(s[i], delta - gamma_new)
    for i in xrange(len(tskset)-1, -1, -1):
        tskset[i].response_time = tskset[i].deadline - s[i]
        if i == 0: break
        s[i-1] = min(s[i], s[i-1])

