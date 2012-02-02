from __future__ import division

import unittest

import schedcat.sched.fp.rta as rta
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
