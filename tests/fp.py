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


# TODO: add tests with blocking and self-suspensions

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
