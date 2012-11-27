from __future__ import division

import unittest

from fractions import Fraction

import schedcat.sched.edf.bak as bak
import schedcat.sched.edf.bar as bar
import schedcat.sched.edf.bcl_iterative as bcli
import schedcat.sched.edf.bcl as bcl
import schedcat.sched.edf.da as da
import schedcat.sched.edf.ffdbf as ffdbf
import schedcat.sched.edf.gfb as gfb
import schedcat.sched.edf.rta as rta
import schedcat.sched.edf as edf

import schedcat.sched as sched

import schedcat.model.tasks as tasks

from schedcat.util.math import is_integral

# TODO: add unit tests for EDF schedulability tests

class DA(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(80, 100),
                tasks.SporadicTask(33, 66),
                tasks.SporadicTask(7, 10),
            ])

    def test_util_bound(self):
        self.assertTrue(da.has_bounded_tardiness(2, self.ts))
        self.assertFalse(da.has_bounded_tardiness(1, self.ts))

    def test_bound_is_integral(self):
        self.assertTrue(da.bound_response_times(2, self.ts))
        self.assertTrue(is_integral(self.ts[0].response_time))
        self.assertTrue(is_integral(self.ts[1].response_time))
        self.assertTrue(is_integral(self.ts[2].response_time))

        self.assertFalse(da.bound_response_times(1, self.ts))

class Test_ffdbf(unittest.TestCase):

    def setUp(self):
        self.t1 = tasks.SporadicTask(5000, 10000)
        self.t2 = tasks.SporadicTask(5000, 10000, deadline = 7000)

    def test_ffdbf1(self):
        one = Fraction(1)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 0, one),
            0)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 5000, one),
            0)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 5001, one),
            1)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 7000, one),
            2000)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 9999, one),
            4999)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 10001, one),
            5000)
        self.assertEqual(
            ffdbf.ffdbf(self.t1, 14001, one),
            5000)

    def test_ffdbf_constrained(self):
        one = Fraction(1)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 0, one),
            0)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 1000, one),
            0)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 2001, one),
            1)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 4000, one),
            2000)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 6999, one),
            4999)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 10001, one),
            5000)
        self.assertEqual(
            ffdbf.ffdbf(self.t2, 12001, one),
            5001)

    def test_test_points(self):
        one = Fraction(1)
        pts = ffdbf.test_points(self.t1, one, 0)
        pts = iter(pts)
        self.assertEqual(pts.next(),  5000)
        self.assertEqual(pts.next(), 10000)
        self.assertEqual(pts.next(), 15000)
        self.assertEqual(pts.next(), 20000)


        pts = ffdbf.test_points(self.t2, one, 0)
        pts = iter(pts)
        self.assertEqual(pts.next(),  2000)
        self.assertEqual(pts.next(),  7000)
        self.assertEqual(pts.next(), 12000)
        self.assertEqual(pts.next(), 17000)


        pts = ffdbf.test_points(self.t1, Fraction(1, 2), 0)
        pts = iter(pts)
        self.assertEqual(pts.next(), 10000)
        self.assertEqual(pts.next(), 10000)
        self.assertEqual(pts.next(), 20000)

        pts = ffdbf.test_points(self.t2, Fraction(8, 10), 0)
        pts = iter(pts)
        self.assertEqual(pts.next(),   750)
        self.assertEqual(pts.next(),  7000)
        self.assertEqual(pts.next(), 10750)
        self.assertEqual(pts.next(), 17000)

    def test_testing_set(self):
        one = Fraction(1)
        ts = tasks.TaskSystem([self.t1, self.t2])
        pts = ffdbf.testing_set(ts, one, 0)
        ts = iter(pts)
        self.assertEqual(pts.next(),  2000)
        self.assertEqual(pts.next(),  5000)
        self.assertEqual(pts.next(),  7000)
        self.assertEqual(pts.next(), 10000)
        self.assertEqual(pts.next(), 12000)
        self.assertEqual(pts.next(), 15000)
        self.assertEqual(pts.next(), 17000)
        self.assertEqual(pts.next(), 20000)


class Test_QPA(unittest.TestCase):

    def setUp(self):
        self.ts =  tasks.TaskSystem([
            tasks.SporadicTask(6000, 31000, deadline=18000),
            tasks.SporadicTask(2000,  9800, deadline= 9000),
            tasks.SporadicTask(1000, 17000, deadline=12000),
            tasks.SporadicTask(  90,  4200, deadline= 3000),
            tasks.SporadicTask(   8,    96, deadline=   78),
            tasks.SporadicTask(   2,    12, deadline=   16),
            tasks.SporadicTask(  10,   280, deadline=  120),
            tasks.SporadicTask(  26,   660, deadline=  160),
            ])

    def test_qpa_schedulable(self):
        qpa = edf.native.QPATest(1)
        self.assertTrue(qpa.is_schedulable(sched.get_native_taskset(self.ts)))

    def test_edf_schedulable(self):
        self.assertTrue(edf.is_schedulable(1, self.ts))

    def test_qpa_not_schedulable(self):
        self.ts.append(tasks.SporadicTask(   10,    100, deadline=15))
        qpa = edf.native.QPATest(1)
        self.assertFalse(qpa.is_schedulable(sched.get_native_taskset(self.ts)))

    def test_edf_schedulable(self):
        self.ts.append(tasks.SporadicTask(   10,    100, deadline=15))
        self.assertFalse(edf.is_schedulable(1, self.ts))
