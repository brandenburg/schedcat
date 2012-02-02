from __future__ import division

from math import trunc

import random

import schedcat.model.tasks as ts

def uniform_int(minval, maxval):
    "Create a function that draws ints uniformly from {minval, ..., maxval}"
    def _draw():
        return random.randint(minval, maxval)
    return _draw

def uniform(minval, maxval):
    "Create a function that draws floats uniformly from [minval, maxval]"
    def _draw():
        return random.uniform(minval, maxval)
    return _draw

def uniform_choice(choices):
    "Create a function that draws uniformly elements from choices"
    selector = uniform_int(0, len(choices) - 1)
    def _draw():
        return choices[selector()]
    return _draw

def truncate(minval, maxval):
    def _limit(fun):
        def _f(*args, **kargs):
            val = fun(*args, **kargs)
            return min(maxval, max(minval, val))
        return _f
    return _limit

def redraw(minval, maxval):
    def _redraw(dist):
        def _f(*args, **kargs):
            in_range = False
            while not in_range:
                val = dist(*args, **kargs)
                in_range = minval <= val <= maxval
            return val
        return _f
    return _redraw

def exponential(minval, maxval, mean, limiter=redraw):
    """Create a function that draws floats from an exponential
    distribution with expected value 'mean'. If a drawn value is less
    than minval or greater than maxval, then either another value is
    drawn (if limiter=redraw) or the drawn value is set to minval or
    maxval (if limiter=truncate)."""
    def _draw():
        return random.expovariate(1.0 / mean)
    return limiter(minval, maxval)(_draw)

def multimodal(weighted_distributions):
    """Create a function that draws values from several distributions
    with probability according to the given weights in a list of
    (distribution, weight) pairs."""
    total_weight = sum([w for (d, w) in weighted_distributions])
    selector = uniform(0, total_weight)
    def _draw():
        x = selector()
        wsum = 0
        for (d, w) in weighted_distributions:
            wsum += w
            if wsum >= x:
                return d()
        assert False # should never drop off
    return _draw



class TaskGenerator(object):
    """Sporadic task generator"""

    def __init__(self, period, util, deadline=lambda x, y: y):
        """Creates TaskGenerator based on a given a period and
        utilization distributions."""
        self.period    = period
        self.util      = util
        self.deadline  = deadline

    def tasks(self, max_tasks=None, max_util=None, squeeze=False,
              time_conversion=trunc):
        """Generate a sequence of tasks until either max_tasks is reached
        or max_util is reached. If max_util would be exceeded and squeeze is
        true, then the last-generated task's utilization is scaled to exactly
        match max_util. Otherwise, the last-generated task is discarded.
        time_conversion is used to convert the generated (non-integral) values
        into integral task parameters.
        """
        count = 0
        usum  = 0
        while ((max_tasks is None or count < max_tasks) and
               (max_util is None  or usum  < max_util)):
            period   = self.period()
            util     = self.util()
            cost     = period * util
            deadline = self.deadline(cost, period)
            # scale as required
            period   = max(1,    int(time_conversion(period)))
            cost     = max(1,    int(time_conversion(cost)))
            deadline = max(1, int(time_conversion(deadline)))
            util = cost / period
            count  += 1
            usum   += util
            if max_util and usum > max_util:
                if squeeze:
                    # make last task fit exactly
                    util -= (usum - max_util)
                    cost = trunc(period * util)
                else:
                    break
            yield ts.SporadicTask(cost, period, deadline)

    def make_task_set(self, *extra, **kextra):
        return ts.TaskSystem(self.tasks(*extra, **kextra))
