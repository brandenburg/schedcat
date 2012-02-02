from __future__ import division

from math    import floor
from fractions import Fraction

from schedcat.util.quantor import forall

ONE = Fraction(1)

def N(t_k, t_i):
    # assumes integral time
    return int(floor((t_k.deadline - t_i.deadline) / t_i.period)) + 1

def beta(t_k, t_i):
    N_i = N(t_k, t_i)
    C_i = t_i.cost
    T_i = t_i.period
    D_k = t_k.deadline
    return Fraction(N_i * C_i + min(C_i, max(0, D_k - N_i * T_i)) , D_k)

def task_schedulable(T, t_k, m):
    l_k = t_k.density_q()
    cap = m * (ONE - l_k)
    all_beta = [beta(t_k, t_i) for t_i in T if t_i != t_k]
    beta_sum = sum([min(b, ONE - l_k) for b in all_beta])
    return beta_sum < cap or \
        (beta_sum == cap and any([0 < b <= ONE - l_k for b in all_beta]))

def is_schedulable(no_cpus, tasks):
    return forall(tasks)(lambda t_k: task_schedulable(tasks, t_k, no_cpus))
