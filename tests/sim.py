from __future__ import division

import unittest

import schedcat.sim.edf as edf
import schedcat.model.tasks as tasks

from schedcat.util.math import is_integral

class EDFSimulator(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(2,  3),
                tasks.SporadicTask(2,  3),
                tasks.SporadicTask(2,  3),
            ])

    def test_deadline_miss(self):
        self.assertTrue(edf.is_deadline_missed(1, self.ts))
        self.assertTrue(edf.is_deadline_missed(2, self.ts))
        self.assertFalse(edf.is_deadline_missed(3, self.ts, simulation_length=1))

    def test_deadline_miss_time(self):
        self.assertEqual(edf.time_of_first_miss(1, self.ts), 3)
        self.assertEqual(edf.time_of_first_miss(2, self.ts), 3)
        self.assertEqual(edf.time_of_first_miss(3, self.ts, simulation_length=1), 0)
