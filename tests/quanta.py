from __future__ import division

import unittest

from fractions import Fraction

import schedcat.overheads.quanta as q
import schedcat.model.tasks as tasks

from schedcat.util.math import is_integral

class Overheads(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(100, 1000),
                tasks.SporadicTask(39, 1050),
                tasks.SporadicTask(51, 599),
            ])
        self.qlen = 50

    def test_wcet(self):
        q.quantize_wcet(self.qlen, self.ts)
        self.assertEqual(self.ts[0].cost, 100)
        self.assertEqual(self.ts[1].cost,  50)
        self.assertEqual(self.ts[2].cost, 100)

        self.assertTrue(is_integral(self.ts[0].cost))
        self.assertTrue(is_integral(self.ts[1].cost))
        self.assertTrue(is_integral(self.ts[2].cost))

        self.assertEqual(self.ts[0].period, 1000)
        self.assertEqual(self.ts[1].period, 1050)
        self.assertEqual(self.ts[2].period, 599)

        self.assertEqual(self.ts[0].deadline, 1000)
        self.assertEqual(self.ts[1].deadline, 1050)
        self.assertEqual(self.ts[2].deadline, 599)

    def test_ewcet(self):
        q.quantize_wcet(self.qlen, self.ts, effective_qlen=25)
        self.assertEqual(self.ts[0].cost, 200)
        self.assertEqual(self.ts[1].cost, 100)
        self.assertEqual(self.ts[2].cost, 150)

        self.assertTrue(is_integral(self.ts[0].cost))
        self.assertTrue(is_integral(self.ts[1].cost))
        self.assertTrue(is_integral(self.ts[2].cost))

        self.assertEqual(self.ts[0].period, 1000)
        self.assertEqual(self.ts[1].period, 1050)
        self.assertEqual(self.ts[2].period, 599)

        self.assertEqual(self.ts[0].deadline, 1000)
        self.assertEqual(self.ts[1].deadline, 1050)
        self.assertEqual(self.ts[2].deadline, 599)

    def test_period(self):
        q.quantize_period(self.qlen, self.ts)
        self.assertEqual(self.ts[0].cost, 100)
        self.assertEqual(self.ts[1].cost,  39)
        self.assertEqual(self.ts[2].cost,  51)

        self.assertTrue(is_integral(self.ts[0].period))
        self.assertTrue(is_integral(self.ts[1].period))
        self.assertTrue(is_integral(self.ts[2].period))

        self.assertEqual(self.ts[0].period, 1000)
        self.assertEqual(self.ts[1].period, 1050)
        self.assertEqual(self.ts[2].period,  550)

        self.assertEqual(self.ts[0].deadline, 1000)
        self.assertEqual(self.ts[1].deadline, 1050)
        self.assertEqual(self.ts[2].deadline,  599)

    def test_release_delay(self):
        q.account_for_delayed_release(101, self.ts)
        q.quantize_period(self.qlen, self.ts)
        self.assertEqual(self.ts[0].cost, 100)
        self.assertEqual(self.ts[1].cost,  39)
        self.assertEqual(self.ts[2].cost,  51)

        self.assertTrue(is_integral(self.ts[0].period))
        self.assertTrue(is_integral(self.ts[1].period))
        self.assertTrue(is_integral(self.ts[2].period))

        self.assertEqual(self.ts[0].period,  850)
        self.assertEqual(self.ts[1].period,  900)
        self.assertEqual(self.ts[2].period,  450)

    def test_staggering(self):
        q.account_for_staggering(self.qlen, 4, self.ts)

        self.assertAlmostEqual(self.ts[0].period,  1000 - 37.5)
        self.assertAlmostEqual(self.ts[1].period,  1050 - 37.5)
        self.assertAlmostEqual(self.ts[2].period,   599 - 37.5)

        self.assertEqual(self.ts[0].cost, 100)
        self.assertEqual(self.ts[1].cost,  39)
        self.assertEqual(self.ts[2].cost,  51)

        q.quantize_period(self.qlen, self.ts)

        self.assertEqual(self.ts[0].period,  950)
        self.assertEqual(self.ts[1].period, 1000)
        self.assertEqual(self.ts[2].period,  550)
