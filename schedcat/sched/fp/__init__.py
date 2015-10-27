"""Fixed-priority schedulability tests.
"""

from __future__ import division

from .rta import bound_response_times as uni_bound_response_times
from .rta import is_schedulable as uni_is_schedulable

from .guan import bound_response_times as global_bound_response_times
from .guan import is_schedulable as global_is_schedulable


def is_schedulable(num_cpus, taskset):
    if num_cpus == 1:
        return uni_is_schedulable(num_cpus, taskset)
    else:
        return global_is_schedulable(num_cpus, taskset)

def bound_response_times(num_cpus, taskset):
    if num_cpus == 1:
        return uni_bound_response_times(num_cpus, taskset)
    else:
        return global_bound_response_times(num_cpus, taskset)
