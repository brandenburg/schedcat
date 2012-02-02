"""
Schedulability test based on:

    Improved multiprocessor global schedulability analysis
    S. Baruah and V. Bonifaci and A. Marchetti-Spaccamela and S. Stiller
    Real-Time Systems, to appear, Springer, 2010.


NB: this code is slow (i.e., of pseudo-polynomial complexity and not optimized),
    and also not well tested.
"""

from __future__ import division

import numbers
from math import trunc
from fractions import Fraction

from schedcat.util.iter import imerge, uniq


def ffdbf(ti, time, speed):
    r_i = time % ti.period
    q_i = trunc(time / ti.period)
    demand = q_i * ti.cost
    if r_i >= ti.deadline:
        demand += ti.cost
    elif ti.deadline > r_i >= ti.deadline - (Fraction(ti.cost) / speed):
        assert isinstance(speed, Fraction)    
        demand += ti.cost - (ti.deadline - r_i) * speed
    # else add nothing
    return demand

def ts_ffdbf(ts, time, speed):
    demand = 0
    for t in ts:
        demand += ffdbf(t, time, speed)
    return demand

def witness_condition(cpus, ts, time, speed):
    "Equ. (6)"
    demand = ts_ffdbf(ts, time, speed)
    bound  = (cpus - (cpus - 1) * speed) * time
    return demand <= bound

def test_points(ti, speed, min_time):
#    assert isinstance(min_time, numbers.Rational)
    skip   = trunc(min_time / ti.period)
    time   = (skip * ti.period) + ti.deadline
    assert isinstance(speed, Fraction)
    offset = min(Fraction(ti.cost) / speed, ti.deadline)

    # check the first two points for exclusion
    if time - offset > min_time:
        yield time - offset
    if time > min_time:
        yield time
    time += ti.period

    while True:
        yield time - offset
        yield time
        time += ti.period

def testing_set(ts, speed, min_time):
    all_points = [test_points(ti, speed, min_time) for ti in ts]
    return uniq(imerge(lambda x,y: x < y, *all_points))

def brute_force_sigma_values(ts, step=Fraction(1,100)):
    maxd = ts.max_density_q()
    yield maxd
    x = (maxd - maxd %  step) + step
    while True:
        yield x
        x += step

def is_schedulable(cpus, ts,
                   epsilon=Fraction(1, 10),
                   sigma_granularity=50):
    if not ts:
        # protect against empty task sets
        return True

    if cpus < 2:
        # sigma bounds requires cpus >= 2
        return False

    assert isinstance(epsilon, Fraction)

    sigma_bound = (cpus - ts.utilization_q()) / Fraction(cpus - 1) - epsilon
    time_bound  = Fraction(sum([ti.cost for ti in ts])) / epsilon
    max_density = ts.max_density_q()

    microsteps     = 0
    sigma_step = Fraction(1, sigma_granularity)

    # sigma is only defined for <= 1
    sigma_bound = min(1, sigma_bound)
    sigma_vals  = iter(brute_force_sigma_values(ts, step=sigma_step))

    schedulable = False
    sigma_cur = sigma_vals.next()
    t_cur = 0

    while not schedulable and max_density <= sigma_cur <= sigma_bound:
        schedulable = True
        for t in testing_set(ts, sigma_cur, t_cur):
            if time_bound < t:
                # great, we made it to the end
                break
            if not witness_condition(cpus, ts, t, sigma_cur):
                # nope, sigma_cur is not a witness
                schedulable = False

                while True:
                    # search next sigma value
                    sigma_nxt = sigma_vals.next()
                    if not (max_density <= sigma_nxt <= sigma_bound):
                        # out of bounds, give up
                        sigma_cur = 2
                        break
                    if witness_condition(cpus, ts, t, sigma_nxt):
                        # this one works
                        sigma_cur = sigma_nxt
                        break

                # don't have to recheck times already checked
                t_cur = t
                break

    return schedulable
