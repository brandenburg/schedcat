"""Response-time analysis for global fixed-priority scheduling according
to Bertogna et al., "Response-Time Analysis for globally scheduled Symmetric
Multiprocessor Platforms", published at RTSS'07.

[EA:09] With extensions from Easwaran and Andersson, "Resource Sharing in Global
Fixed-Priority Preemptive Multiprocessor Scheduling", RTSS'09.
"""

from __future__ import division

from math import floor

def is_schedulable(num_cpus, tasks, **kargs):
    return all(rta_schedulable(k, tasks, num_cpus, **kargs) for k in xrange(len(tasks)))

bound_response_times = is_schedulable

def workload_function(task, time):
    # Calculate higher-priority workload during the time interval 'time'.
    # Eq. 3 in Bertogna's RTSS'07 paper
    # NiL = lfloor (L + Di - Ci) / Ti rfloor
    njobs = floor((time - task.cost + task.deadline) / task.period)

    # Eq. 4 in Bertogna's RTSS'07 paper
    # NiL * Ci + min(Ci, L + Di - Ci - NiL * Ti)
    carry_in = min(task.cost,
                   time - task.cost + task.deadline - task.period * njobs)

    workload = task.cost * njobs + carry_in
    return workload

def slack_aware_workload_function(task, time):
    # Calculate higher-priority workload during the time interval 'time'.

    # Slack of the interfering task
    slack = max(0, task.deadline - task.response_time)

    # Eq. 8 in Bertogna's RTSS'07 paper
    # NiL = lfloor (L + Di - Ci - si) / Ti rfloor
    njobs = floor((time - task.cost - slack + task.deadline) / task.period)

    # Still Eq. 8 in Bertogna's RTSS'07 paper
    # NiL * Ci + min(Ci, L + Di - Ci - si - NiL * Ti)
    carry_in = min(task.cost,
                   time - task.cost + task.deadline - slack
                        - task.period * njobs)

    workload = task.cost * njobs + carry_in
    return workload

def rta_schedulable(i, taskset, num_cpus, dont_use_slack=False):
    task = taskset[i]

    # EA:09 extension: add blocking terms to response-time bound
    blocked = task.blocked if 'blocked' in task.__dict__ else 0

    # The m highest priority tasks do not subject
    # to higher-priority interference
    # (tasks are indexed by decreasing priorities)
    if i < num_cpus:
        if task.cost + blocked > task.deadline:
            return False
        else:
            task.response_time = task.cost + blocked
            return True

    higher_prio = taskset[:i]

    test_end = task.deadline
    # Starting from ei + bi
    time = task.cost + blocked

    # see if we find a point where the demand is satisfied
    while time <= test_end:

        # Calculate higher-priority workload
        if dont_use_slack:
            hp_interference = sum([workload_function(t, time) for t in higher_prio])
        else:
            hp_interference = sum([slack_aware_workload_function(t, time)
                                   for t in higher_prio])

        # EA:09 extension: if higher-priority direct-blocking demand
        # is already part of the blocking term, we don't have to add
        # it again to the "regular" interference bound.
        # Check if a specially named higher-priority direct blocking
        # bound exists as a task attribute.
        if 'hp_direct_blocked' in task.__dict__:
            hp_interference -= task.hp_direct_blocked
            
            # The direct blocking from higher-priority tasks cannot exceed
            # the regular interference. Hence, hp_interference is set to at
            # least 0. Otherwise, hp_interference can potentially become
            # negative if the task.response_time is manually set to 
            # task.deadline, the blocking analysis determines a blocking
            # bound using task.response_time as a basis, while this analysis
            # here calculates the interference based on the response-time
            # it computed itself, hence ignoring the response-time set
            # manually.
            hp_interference = max(hp_interference, 0)

        hp_interference = int(floor(hp_interference / num_cpus))

        demand = task.cost + blocked + hp_interference

        if demand == time:
            # yep, demand will be met by time
            task.response_time = time
            return True
        else:
            # try again
            time = demand

    # if we get here, we didn't converge

    return False
