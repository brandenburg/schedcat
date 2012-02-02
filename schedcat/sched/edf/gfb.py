from __future__ import division

# The G-EDF density test.
def is_schedulable(no_cpus, tasks):
    """Is the system schedulable according to the GFB test?
    Also known as the "density test."
    """
    if not tasks:
        return True
    dmax  = max([t.density() for t in tasks])
    bound = no_cpus - (no_cpus - 1) * dmax
    return tasks.density() <= bound
