from __future__ import division

import unittest

import schedcat.sched.fp.rta as rta
import schedcat.sched.fp.bertogna as ber
import schedcat.sched.fp as fp

import schedcat.model.tasks as tasks

from schedcat.util.math import is_integral

class UniprocessorRTA(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(3, 18),
            ])

    def test_procs(self):
        self.assertTrue(rta.is_schedulable(1, self.ts))
        self.assertFalse(rta.is_schedulable(2, self.ts))

    def test_bound_is_integral(self):
        self.assertTrue(rta.is_schedulable(1, self.ts))
        self.assertTrue(is_integral(self.ts[0].response_time))
        self.assertTrue(is_integral(self.ts[1].response_time))
        self.assertTrue(is_integral(self.ts[2].response_time))

    def test_times(self):
        self.assertTrue(rta.is_schedulable(1, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  2)
        self.assertEqual(self.ts[2].response_time,  7)
        self.assertEqual(self.ts[3].response_time, 18)


class UniprocessorSelfSuspensions(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,   2),
                tasks.SporadicTask(5,  20),
                tasks.SporadicTask(1,  20),
            ])

    def test_times_no_suspension(self):
        self.assertTrue(rta.is_schedulable(1, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time, 10)
        self.assertEqual(self.ts[2].response_time, 12)

    def test_times_with_suspension(self):
        self.ts[1].suspended = 5
        self.assertFalse(rta.is_schedulable(1, self.ts))
        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time, 20)

    def test_times_with_legacy_suspension(self):
        self.ts[1].blocked = 5
        self.ts[1].suspended = 5
        self.assertFalse(rta.is_schedulable(1, self.ts))
        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time, 20)

    def test_times_with_legacy_blocked(self):
        self.ts[0].blocked = 1
        self.ts[1].blocked = 5
        self.assertTrue(rta.is_schedulable(1, self.ts))
        self.assertEqual(self.ts[0].response_time,  2)
        self.assertEqual(self.ts[1].response_time, 20)
        self.assertEqual(self.ts[2].response_time, 12)

class AudsleyExample(unittest.TestCase):
    def setUp(self):
        example_tasks = [
            (51, 1000, 1000, 0, 0, 51),
            (3000, 2000000, 5000, 300, 0, 3504),
            (2000, 25000, 25000, 600, 0, 5906),
            (5000, 25000, 25000, 900, 0, 11512),
            (1000, 40000, 40000, 1350, 0, 13064),
            (3000, 50000, 50000, 1350, 0, 16217),
            (5000, 50000, 50000, 750, 0, 20821),
            (8000, 59000, 59000, 750, 0, 36637),
            (9000, 80000, 80000, 1350, 0, 47798),
            (2000, 80000, 80000, 450, 0, 48949),
            (5000, 100000, 100000, 1050, 0, 99150),
            (1000, 200000, 200000, 450, 1000, 99550),
            (3000, 200000, 200000, 450, 0, 140641),
            (1000, 200000, 200000, 450, 0, 141692),
            (1000, 200000, 200000, 1350, 0, 143694),
            (3000, 1000000, 1000000, 0, 0, 145446),
            (1000, 1000000, 1000000, 0, 0, 146497),
            (1000, 1000000, 1000000, 0, 0, 147548),
        ]
        self.ts = tasks.TaskSystem()
        for (cost, period, deadline, blocking, jitter, expected) in example_tasks:
            t = tasks.SporadicTask(cost, period, deadline)
            t.jitter = jitter
            t.pcp = blocking
            t.expected = expected + jitter
            self.ts.append(t)

    def test_times_with_legacy_blocked(self):
        for t in self.ts:
            t.blocked = t.pcp
        self.assertTrue(rta.is_schedulable(1, self.ts))
        for t in self.ts:
            self.assertEqual(t.response_time, t.expected)

    def test_times(self):
        for t in self.ts:
            t.prio_inversion = t.pcp
        self.assertTrue(rta.is_schedulable(1, self.ts))
        for t in self.ts:
            self.assertEqual(t.response_time, t.expected)


class MultiprocessorRTA(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(9, 18),
            ])

    def test_procs(self):
        self.assertFalse(fp.is_schedulable(1, self.ts))
        self.assertTrue(fp.is_schedulable(2, self.ts))

    def test_bound_is_integral(self):
        self.assertTrue(fp.is_schedulable(2, self.ts))
        self.assertTrue(is_integral(self.ts[0].response_time))
        self.assertTrue(is_integral(self.ts[1].response_time))
        self.assertTrue(is_integral(self.ts[2].response_time))
        self.assertTrue(is_integral(self.ts[3].response_time))

    def test_times(self):
        self.assertTrue(fp.is_schedulable(2, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  4)
        self.assertEqual(self.ts[3].response_time, 15)

    def test_times2(self):
        self.assertTrue(fp.bound_response_times(2, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  4)
        self.assertEqual(self.ts[3].response_time, 15)

    def test_times3(self):
        self.assertTrue(fp.bound_response_times(3, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  3)
        self.assertEqual(self.ts[3].response_time, 12)


class BertognaRTA(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(9, 18),
            ])

    def test_procs(self):
        self.assertFalse(ber.is_schedulable(1, self.ts, dont_use_slack=True))
        self.assertFalse(ber.is_schedulable(2, self.ts, dont_use_slack=True))
        self.assertTrue(ber.is_schedulable(3, self.ts, dont_use_slack=True))

    def test_bound_is_integral(self):
        self.assertTrue(ber.is_schedulable(3, self.ts, dont_use_slack=True))
        self.assertTrue(is_integral(self.ts[0].response_time))
        self.assertTrue(is_integral(self.ts[1].response_time))
        self.assertTrue(is_integral(self.ts[2].response_time))
        self.assertTrue(is_integral(self.ts[3].response_time))

    def test_times(self):
        self.assertTrue(ber.is_schedulable(3, self.ts, dont_use_slack=True))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  3)
        self.assertEqual(self.ts[3].response_time, 14)

    def test_times2(self):
        self.assertTrue(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  3)
        self.assertEqual(self.ts[3].response_time, 14)

    def test_times3(self):
        self.assertTrue(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  3)
        self.assertEqual(self.ts[3].response_time, 14)

    def test_times4(self):
        # with slack, we need one fewer core
        self.assertTrue(ber.bound_response_times(2, self.ts))

        self.assertEqual(self.ts[0].response_time,  1)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  4)
        self.assertEqual(self.ts[3].response_time, 15)

    def test_blocking(self):
        # made up values, to see if they get picked up
        self.ts[0].blocked = 2
        self.ts[2].blocked = 3
        self.ts[3].hp_direct_blocked = 4

        self.assertTrue(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.assertEqual(self.ts[0].response_time,  3)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  6)
        self.assertEqual(self.ts[3].response_time, 12)

    def test_infeasible_blocking(self):
        # made up values, to see if they get picked up
        self.ts[0].blocked = 5
        self.assertFalse(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.ts[0].blocked = 2
        self.ts[3].blocked = 10
        self.assertFalse(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.ts[3].blocked = 3
        self.assertTrue(ber.bound_response_times(3, self.ts, dont_use_slack=True))

        self.assertEqual(self.ts[0].response_time,  3)
        self.assertEqual(self.ts[1].response_time,  1)
        self.assertEqual(self.ts[2].response_time,  3)
        self.assertEqual(self.ts[3].response_time, 18)


