from __future__ import division

from math import ceil

# task.prio_inversion => LOCAL blocking (think PCP or SRP)
# task.suspended => self-suspensions, e.g. as caused by REMOTE blocking
# task.jitter    => delay between arrival and release of job

# legacy code also sets task.blocked => sum of ALL blocking, including
# self-suspensions, spinning, transitive spinning, and what not.
# Tasks with task.blocked set are handled specially to account for this quirk.

# Since any of these fields may not be set, we define custom getters.

def get_blocked(task):
    return task.__dict__.get('blocked', 0)

def get_jitter(task):
    return task.__dict__.get('jitter', 0)

def get_suspended(task):
    return task.__dict__.get('suspended', 0)

def get_prio_inversion(task):
    return task.__dict__.get('prio_inversion', 0)

def suspension_jitter(task):
    if get_suspended(task) > 0:
        # suspension to jitter reduction: max jitter is R_i - C_i.
        return task.response_time - task.cost
    else:
        return get_jitter(task)

def _rta_jitter_aware(task, own_demand, higher_prio_tasks, hp_jitter):
    # see if we find a point where the demand is satisfied
    delta = sum([t.cost for t in higher_prio_tasks]) + own_demand
    while delta <= task.deadline:
        demand = own_demand
        for t in higher_prio_tasks:
            demand += t.cost * int(ceil((delta + hp_jitter(t)) / t.period))
        if demand == delta:
            # yep, demand will be met by time
            task.response_time = delta + get_jitter(task)
            return True
        else:
            # try again
            delta = demand
    # if we get here, we didn't converge
    return False

def rta_jitter_aware(task, higher_prio_tasks):
    # local blocking B_i and own WCET C_i
    own_demand = get_prio_inversion(task) + task.cost
    return _rta_jitter_aware(task, own_demand, higher_prio_tasks, get_jitter)

def rta_suspension_aware(task, higher_prio_tasks):
    # analysis of self-suspensions by reduction to jitter
    # own self-suspension is counted as exec. time
    own_demand = get_prio_inversion(task) + task.cost + get_suspended(task)
    # higher-prio tasks are considered to have jitter based on response time
    return _rta_jitter_aware(task, own_demand, higher_prio_tasks, suspension_jitter)

def legacy_rta_jitter_aware(task, higher_prio_tasks):
    # legacy code sets task.blocked to the sum of *all* blocking
    own_demand = get_blocked(task) + task.cost
    return _rta_jitter_aware(task, own_demand, higher_prio_tasks, get_jitter)

def legacy_rta_suspension_aware(task, higher_prio_tasks):
    # Legacy code sets task.blocked to the sum of *all* blocking, including
    # the self-suspension time due to remote blocking.
    # This means we do not have to add in the self-suspension time explicitly.
    own_demand = get_blocked(task) + task.cost
    return _rta_jitter_aware(task, own_demand, higher_prio_tasks, suspension_jitter)

def has_self_suspensions(taskset):
    for t in taskset:
        if 'suspended' in t.__dict__ and t.suspended != 0:
            return True
    return False

def uses_legacy_blocked_field(taskset):
    for t in taskset:
        if 'blocked' in t.__dict__:
            return True
    return False

def bound_response_times(no_cpus, taskset):
    # A bit of a kludge: to accomodate legacy code, we check
    # whether the task set uses the old .blocked model or the explicit
    # .prio_inversion + .suspended model.
    legacy = uses_legacy_blocked_field(taskset)
    susp   = has_self_suspensions(taskset)
    """Assumption: taskset is sorted in order of decreasing priority."""
    if not (no_cpus ==  1 and taskset.only_constrained_deadlines()):
        # This implements standard uniprocessor response-time analysis, which
        # does not handle arbitrary deadlines or multiprocessors.
        return False
    elif legacy and susp:
        rta = legacy_rta_suspension_aware
    elif legacy:
        rta = legacy_rta_jitter_aware
    elif susp:
        rta = rta_suspension_aware
    else:
        rta = rta_jitter_aware
    for i, t in enumerate(taskset):
        if not rta(t, taskset[0:i]):
            return False
    return True

is_schedulable = bound_response_times
