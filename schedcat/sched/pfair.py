from schedcat.util.quantor import forall

def is_schedulable(no_cpus, tasks):
    """Simple utilization bound: tasks.utilization() <= no_cpus.
    Assumption: all parameters are quantum multiples and deadlines
    are not constrained.
    """
    return tasks.utilization() <= no_cpus and \
        forall(tasks)(lambda t: t.deadline >= t.period >= t.cost)

def has_bounded_tardiness(no_cpus, tasks):
    """Simple utilization bound: tasks.utilization() <= no_cpus.
    This is also true for constrained-deadline tasks.
    """
    return tasks.utilization() <= no_cpus and \
        forall(tasks)(lambda t: t.period >= t.cost)

def bound_response_times(no_cpus, tasks):
    """Upper bound the response time of each task.
    This assumes that all task parameters are quantum multiples, and
    that effects such as quantum staggering have already been accounted for.
    """
    if has_bounded_tardiness(no_cpus, tasks):
        for t in tasks:
            t.response_time = t.period
        return True
    else:
        return False
