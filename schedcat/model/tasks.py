from __future__ import division # use sane division semantics

import copy

from itertools import count, takewhile, dropwhile
from schedcat.util.iter import uniq
from heapq import merge

from math   import floor, ceil, sqrt
from schedcat.util.math    import lcm

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

    def slack(self):
        """Return this task's slack time.
        Note: this function can only be called after some test
        established a response time bound (response_time must be defined)!
        """
        return self.deadline - self.response_time

    def maxjobs(self, interval_length):
        """Compute the maximum number of jobs that can execute during
        some interval.
        Note: this function can only be called after some test
        established a response time bound (response_time must be defined)!
        """
        return int(ceil((interval_length + self.response_time) / self.period))

    def dbf(self, t):
        """Baruah's Demand Bound Function"""
        if t <= 0:
            return 0
        return max(0, (int(floor((t - self.deadline) / self.period)) + 1)
                      * self.cost)

    def rbf(self, t):
        """Request Bound Function"""
        if t < 0:
            return 0
        return (int(floor(t / self.period)) + 1) * self.cost

    def dbf_points_of_change(self, max_t = None, offset = 0):
        """Return iterator over t where tsk.dbf(t) changes."""
        pts = count(self.deadline - offset, self.period)
        if offset > 0:
            pts = dropwhile(lambda pt: pt < 0, pts)
        if not max_t is None:
            pts = takewhile(lambda pt: pt <= max_t, pts)
        return pts

    def rbf_points_of_change(self, max_t = None, offset = 0):
        """Return iterator over t where tsk.rbf(t) changes."""
        pts = count(-offset, self.period)
        if offset > 0:
            pts = dropwhile(lambda pt: pt < 0, pts)
        if not max_t is None:
            pts = takewhile(lambda pt: pt <= max_t, pts)
        return pts

    def period_transform(self, num_subjobs, want_integer=True):
        self.period   = self.period   / num_subjobs
        self.cost     = self.cost     / num_subjobs
        self.deadline = self.deadline / num_subjobs
        if want_integer:
            self.period   = int(floor(self.period))
            self.cost     = int(ceil(self.cost))
            self.deadline = int(floor(self.deadline))

    def is_feasible(self):
        return self.cost <= min(self.deadline, self.period)

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
        return all(t.implicit_deadline() for t in self)

    def only_constrained_deadlines(self):
        return all(t.constrained_deadline() for t in self)

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

    def sort_by_tkc(self, m):
        """Sort task set by Andersson and Jonsson's TkC heuristic.
        See: B. Andersson, J. Jonsson, "Fixed-priority preemptive
        multiprocessor scheduling: to partition or not to partition",
        RTCSA'00. """
        k = (m - 1 + sqrt(5 * m**2 - 6 * m + 1)) / (2 * m)
        self.sort(key=lambda t: t.period - k*t.cost)

    def sort_by_dkc(self, m):
        """Sort task set by Davis and Burn's DkC heuristic.
        See: R. Davis, A. Burns, "Priority Assignment for Global Fixed
        Priority Pre-emptive Scheduling in Multiprocessor Real-Time
        Systems", RTSS'09. """
        k = (m - 1 + sqrt(5 * m**2 - 6 * m + 1)) / (2 * m)
        self.sort(key=lambda t: t.deadline - k * t.cost)

    def sort_by_RM_US(self, m):
        """Sort task set by Andersson et al.'s RM-US heuristic.
        See: B. Andersson, S. Baruah, and J. Jonsson. "Static-priority
        scheduling on multiprocessors. In Proc. 22nd IEEE Real-Time
        Systems Symposium, RTSS'01. """
        threshold = m / (3 * m - 2)
        self.sort(key= lambda t: 0 if t.utilization() > threshold else t.period)

    def sort_by_DM_US(self, m):
        """Sort task set by DM-US heuristic.
        See: L. Lundberg and H. Lennerstad. "Guaranteeing response times
        for aperiodic tasks in global multiprocessor scheduling." Real-
        Time Syst., 35(2):135-151, 2007. """
        threshold = (m * (3 * m - 2 - sqrt(7 * m**2 - 8 * m + 2))
                     / (m - 1)**2)
        self.sort(key= lambda t: 0 if t.utilization() > threshold else t.period)

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

    def dbf(self, delta):
        """
        Demand bound function of the system in time interval delta.
        """
        return sum((t.dbf(delta) for t in self))

    def rbf(self, delta):
        """
        Request bound function of the system in time interval delta.
        """
        return sum((t.rbf(delta) for t in self))

    def dbf_points_of_change(self, max_t = None, offset = 0):
        all_pts = [t.dbf_points_of_change(max_t, offset) for t in self]
        return uniq(merge(*all_pts))

    def rbf_points_of_change(self, max_t = None, offset = 0):
        all_pts = [t.rbf_points_of_change(max_t, offset) for t in self]
        return uniq(merge(*all_pts))

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
        return min((t.deadline for t in self))

    def max_deadline(self):
        return max((t.deadline for t in self))

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

