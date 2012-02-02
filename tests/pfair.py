from __future__ import division

import unittest

from fractions import Fraction

import schedcat.sched.pfair as p
import schedcat.model.tasks as tasks

class Pfair(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(80, 100),
                tasks.SporadicTask(33, 66),
                tasks.SporadicTask(7, 10),
            ])

    def test_bound(self):
        self.assertTrue(p.is_schedulable(2, self.ts))
        self.assertFalse(p.is_schedulable(1, self.ts))

    def test_deadlines(self):
        self.ts[0].deadline = 300
        self.ts[2].deadline = 11

        self.assertTrue(p.is_schedulable(2, self.ts))
        self.assertFalse(p.is_schedulable(1, self.ts))

        self.ts[1].deadline = 50

        self.assertFalse(p.is_schedulable(2, self.ts))
        self.assertFalse(p.is_schedulable(1, self.ts))

        self.assertTrue(p.has_bounded_tardiness(2, self.ts))
        self.assertFalse(p.has_bounded_tardiness(1, self.ts))

    def test_tardiness(self):
        self.ts[0].deadline = 300
        self.ts[1].deadline = 50
        self.ts[2].deadline = 11

        self.assertTrue(p.bound_response_times(2, self.ts))

        self.assertEqual(self.ts[0].tardiness(),  0)
        self.assertEqual(self.ts[1].tardiness(), 16)
        self.assertEqual(self.ts[2].tardiness(),  0)

        self.assertFalse(p.bound_response_times(1, self.ts))
