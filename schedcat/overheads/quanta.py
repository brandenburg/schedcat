"""Support for quantum-based scheduling.
"""
from __future__ import division

from math      import ceil, floor

def is_quantum_multiple(qlen, value):
    return value % qlen is 0

def has_integral_period(qlen):
    return lambda t: t.period % qlen is 0

def quantize_wcet(qlen, tasks, effective_qlen=None):
    """Round up execution cost to account for partially used quanta.
    Specify an effective_qlen less than the quantum length to account for
    overheads.
    """
    if effective_qlen is None:
        effective_qlen = qlen
    assert effective_qlen > 0
    assert qlen > 0
    for t in tasks:
        nr_quanta = int(ceil(t.cost / effective_qlen))
        t.cost = nr_quanta * qlen
        if t.density() >= 1:
            return False
    return tasks

def quantize_period(qlen, tasks, deadline=False):
    """Round down periods to account for the fact that in a quantum-based
    scheduler all periods must be multiples of the quantum length.

    Rounding down the period of a periodic task yields a sporadic task that has
    an inter-arrival delay of one quantum.
    """
    for t in tasks:
        if not is_quantum_multiple(t.period, qlen):
            nr_quanta = int(floor(t.period / qlen))
            per = nr_quanta * qlen
            t.period = per
        if deadline and not is_quantum_multiple(t.deadline, qlen):
            nr_quanta = int(floor(t.deadline / qlen))
            dl = nr_quanta * qlen
            t.deadline = dl
        if t.density() >= 1:
            return False
    return tasks

def account_for_delayed_release(delay, tasks):
    """A release will not be noticed until the start of the next quantum
    boundary. Hence, the period and deadline must both be reduced by one
    quantum size for hard real-time use.
    """
    for t in tasks:
        t.period   -= delay
        t.deadline -= delay
        if t.density() >= 1:
            return False
    return tasks

def stagger_latency(qlen, num_cpus):
    return (num_cpus - 1) / num_cpus * qlen

def account_for_staggering(qlen, num_cpus, tasks):
    """A job may miss its deadline by up to ((m - 1) / m) of a quantum length
    due to staggering. Hence, we need to reduce the period and deadline.
    
    This leaves non-integral task parameters, which must be quantized
    afterward with quantize_period().
    """
    reduction = stagger_latency(qlen, num_cpus)
    for t in tasks:
        t.period   -= reduction
        t.deadline -= reduction
        if t.density() >= 1:
            return False
    return tasks
