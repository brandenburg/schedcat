from __future__ import division # use sane division semantics

import copy

from math   import floor, ceil
from schedcat.util.math    import lcm
from schedcat.util.quantor import forall

from fractions import Fraction

class SporadicTask(object):
    def __init__(self, exec_cost, period, deadline=None, id=None):
        """By default, the construct only creates the bare minimum
        attributes. Other code (or subclasses) can add additional
        attributes (such as response time bounds, resource usage, etc.)
        """
        if deadline is None:
            # implicit deadline by default
            deadline = period
        self.period     = period
        self.cost       = exec_cost
        self.deadline   = deadline
        self.id         = id

    def implicit_deadline(self):
        return self.deadline == self.period

    def constrained_deadline(self):
        return self.deadline <= self.period

    def utilization(self):
        return self.cost / self.period

    def utilization_q(self):
        return Fraction(self.cost, self.period)

    def density(self):
        return self.cost / min(self.period, self.deadline)

    def density_q(self):
        return Fraction(self.cost, min(self.period, self.deadline))

    def tardiness(self):
        """Return this task's tardiness.
        Note: this function can only be called after some test
        established a response time bound (response_time must be defined)!
        """
        return max(0, self.response_time - self.deadline)

    def maxjobs(self, interval_length):
        """Compute the maximum number of jobs that can execute during
        some interval.
        Note: this function can only be called after some test
        established a response time bound (response_time must be defined)!
        """
        return int(ceil((interval_length + self.response_time) / self.period))

    def __repr__(self):
        idstr = ", id=%s" % self.id if self.id is not None else ""
        dstr  = ", deadline=%s" % self.deadline if self.deadline != self.period else ""
        return "SporadicTask(%s, %s%s%s)" % (self.cost, self.period, dstr, idstr)


class TaskSystem(list):
    def __init__(self, tasks=[]):
        self.extend(tasks)

    def __str__(self):
        return "\n".join([str(t) for t in self])

    def __repr__(self):
        return "TaskSystem([" + ", ".join([repr(t) for t in self]) + "])"

    def only_implicit_deadlines(self):
        return forall(self)(lambda t: t.implicit_deadline())

    def only_constrained_deadlines(self):
        return forall(self)(lambda t: t.constrained_deadline())

    def assign_ids(self):
        for i, t in enumerate(self):
            t.id = i + 1

    def assign_ids_by_period(self):
        for i, t in enumerate(sorted(self, key=lambda t: t.period)):
            t.id = i + 1

    def assign_ids_by_deadline(self):
        for i, t in enumerate(sorted(self, key=lambda t: t.deadline)):
            t.id = i + 1

    def sort_by_period(self):
        self.sort(key=lambda t: t.period)

    def sort_by_deadline(self):
        self.sort(key=lambda t: t.deadline)

    def utilization(self, heaviest=None):
        u = [t.utilization() for t in self]
        if heaviest is None:
            return sum(u)
        else:
            u.sort(reverse=True)
            return sum(u[:heaviest])

    def utilization_q(self, heaviest=None):
        u = [t.utilization_q() for t in self]
        if heaviest is None:
            return sum(u)
        else:
            u.sort(reverse=True)
            return sum(u[:heaviest])

    def density(self):
        return sum([t.density() for t in self])

    def density_q(self):
        return sum([t.density_q() for t in self])

    def hyperperiod(self):
        return lcm(*[t.period for t in self])

    def max_utilization(self):
        return max([t.utilization() for t in self])

    def max_density(self):
        return max([t.density() for t in self])

    def max_density_q(self):
        return max([t.density_q() for t in self])

    def max_cost(self):
        return max([t.cost for t in self])

    def max_period(self):
        return max([t.period for t in self])

    def min_deadline(self):
        return min([t.deadline for t in self])

    def max_wss(self):
        "Assumes t.wss has been initialized for each task."
        return max([t.wss for t in self])

    def copy(self):
        ts = TaskSystem((copy.deepcopy(t) for t in self))
        return ts

    def without(self, excluded_tasks):
        "Iterate over contained tasks, skipping over excluded"
        if isinstance(excluded_tasks, SporadicTask):
            # special case: single argument is a task => singleton set
            return (task for task in self if task != excluded_tasks)
        else:
            # general case: caller provided set of tasks to be excluded
            return (task for task in self if not task in excluded_tasks)

    def with_higher_priority_than(self, lower):
        """Iterate over contained tasks, skipping over tasks with priority
        lower than lower.id (i.e., over tasks with larger indices.).
        """
         # assumption: lower id == higher priority
        return (task for task in self if task.id < lower.id)

    def with_lower_priority_than(self, upper):
        """Iterate over contained tasks, skipping over tasks with priority
        higher than upper.id (i.e., over tasks with smaller indices.).
        """
         # assumption: lower id == higher priority
        return (task for task in self if task.id > upper.id)
