"""EDF hard and soft schedulability tests,
for uni- and multiprocessors.
"""
from __future__ import division

from .gfb import is_schedulable as is_schedulable_gfb

from .gfb import is_schedulable as gfb_test
from .bak import is_schedulable as bak_test
from .bar import is_schedulable as bar_test
from .bcl import is_schedulable as bcl_test
from .bcl_iterative import is_schedulable as bcli_test
from .rta import is_schedulable as rta_test
from .ffdbf import is_schedulable as ffdbf_test

from .da import bound_response_times as da_tardiness_bounds
from .rta import bound_response_times as rta_response_times

from schedcat.util.quantor import forall

# hard real-time tests
HRT_TESTS = {
    'GFB'    : gfb_test,
    'BAK'    : bak_test,
    'BAR'    : bar_test,
    'BCL'    : bcl_test,
    'BCLI'   : bcli_test,
    'RTA'    : rta_test,
    'FF-DBF' : ffdbf_test,
    }

# A somewhat arbitrary heuristic to curb pseudo-polynomial runtimes...
def should_use_baruah_test(threshold, taskset, no_cpus):
    if threshold is True:
        return True
    elif threshold is False:
        return False
    else:
        slack = no_cpus - taskset.utilization()
        if not slack:
            # can't apply test for zero slack; avoid division by zero
            return False
        n     = len(taskset)
        score = n * (no_cpus * no_cpus) / (slack * slack)
        return score <= threshold

# all (pure Python implementation)
def is_schedulable_py(no_cpus, tasks,
                      rta_min_step=1,
                      want_baruah=3000,
                      want_rta=True,
                      want_ffdbf=False,
                      want_load=False):
    if tasks.utilization() > no_cpus or \
        not forall(tasks)(lambda t: t.period >= t.cost):
        # trivially infeasible
        return False
    else:
        not_arbitrary = tasks.only_constrained_deadlines()
        if no_cpus == 1 and tasks.density() <= 1:
            # simply uniprocessor density condition
            return True
        elif no_cpus > 1:
            # Baker's test can handle arbitrary deadlines.
            if bak_test(no_cpus, tasks):
                return True
            # The other tests cannot.
            if not_arbitrary:
                # The density test is cheap, try it first.
                if gfb_test(no_cpus, tasks):
                    return True
                # Ok, try the slower ones.
                if should_use_baruah_test(want_baruah, tasks, no_cpus) and \
                    bar_test(no_cpus, tasks):
                    return True
                if want_rta and \
                    rta_test(no_cpus, tasks,
                             min_fixpoint_step=rta_min_step):
                    return True
                # FF-DBF is almost always too slow.
                if want_ffdbf and ffdbf_test(no_cpus, tasks):
                    return True
    # If we get here, none of the tests passed.
    return False

import schedcat.sched

if schedcat.sched.using_native:
    import schedcat.sched.native as native

    def is_schedulable_cpp(no_cpus, tasks,
                           rta_min_step=1,
                           want_baruah=True,
                           want_rta=True,
                           want_ffdbf=False,
                           want_load=False):
        if no_cpus == 1:
            native_test = native.QPATest(no_cpus);
        else:
            native_test = native.GlobalEDF(no_cpus, rta_min_step,
                                           want_baruah != False,
                                           want_rta,
                                           want_ffdbf,
                                           want_load)
        ts = schedcat.sched.get_native_taskset(tasks)
        return native_test.is_schedulable(ts)

    is_schedulable = is_schedulable_cpp

else:
    is_schedulable = is_schedulable_py


def bound_response_times(no_cpus, tasks, *args, **kargs):
    if is_schedulable(no_cpus, tasks, *args, **kargs):
        # HRT schedualble => no tardiness
        # See if we can get RTA to provide a good response time bound.
        rta_step = kargs['rta_min_step'] if 'rta_min_step' in kargs else 0
        if rta_response_times(no_cpus, tasks, min_fixpoint_step=rta_step):
            # Great, we got RTA estimates.
            return True
        else:
            # RTA failed, use conservative bounds.
            for t in tasks:
                t.response_time = t.deadline
            return True
    # Not HRT schedulable, use SRT analysis.
    elif da_tardiness_bounds(no_cpus, tasks):
        return True
    else:
        # Could not find a safe response time bound for each task.
        return False
