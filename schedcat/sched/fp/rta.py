from __future__ import division

from math import ceil

from schedcat.util.quantor import forall

# task.blocked   => ALL blocking, including local and remote (self-suspensions)
# task.suspended => self-suspensions, aka only REMOTE blocking
# task.jitter    => ADDITIONAL self-suspensions (not included in task.blocked or task.suspended)

def check_for_suspension_parameters(taskset):
    "compatibility: add required parameters if they are not present"
    for t in taskset:
        if not 'blocked' in t.__dict__:
            # No blocking.
            t.blocked = 0
        if not 'suspended' in t.__dict__:
            # No self-suspension time.
            t.suspended = 0
        if not 'jitter' in t.__dict__:
            # No arrival jitter (equivalent to an initial suspension).
            t.jitter = 0

def fp_demand(task, time):
    # Account for higher-priority interference due to double-hit /
    # back-to-back execution.
    return task.cost * int(ceil((time + task.suspended + task.jitter) / task.period))

def rta_schedulable(taskset, i):
    task = taskset[i]
    higher_prio = taskset[:i]

    test_end = task.deadline

    # pre-compute the additional terms for the processor demand bound
    own_demand = task.blocked + task.jitter + task.cost

    # see if we find a point where the demand is satisfied
    time = sum([t.cost for t in higher_prio]) + own_demand
    while time <= test_end:
        demand = sum([fp_demand(t, time) for t in higher_prio]) \
            + own_demand
        if demand == time:
            # yep, demand will be met by time
            task.response_time = time
            return True
        else:
            # try again
            time = demand

    # if we get here, we didn't converge
    return False

def bound_response_times(no_cpus, taskset):
    """Assumption: taskset is sorted in order of decreasing priority."""
    if not (no_cpus ==  1 and taskset.only_constrained_deadlines()):
        # This implements standard uniprocessor response-time analysis, which
        # does not handle arbitrary deadlines or multiprocessors.
        return False
    else:
        check_for_suspension_parameters(taskset)
        for i in xrange(len(taskset)):
            if not rta_schedulable(taskset, i):
                return False
        return True

is_schedulable = bound_response_times


