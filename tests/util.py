from __future__ import division

import unittest

from fractions import Fraction

import schedcat.util.iter as iter
import schedcat.util.math as m

class Iters(unittest.TestCase):
    def setUp(self):
        self.s1 = xrange(1, 1000, 3)
        self.s2 = xrange(4, 1000, 5)
        self.s3 = [-3, 6000]
        self.s1b = xrange(1, 1000, 3)
        self.s1c = xrange(1, 1000, 3)

    def test_imerge(self):
        s = iter.imerge(lambda x, y: x < y, self.s1, self.s2, self.s3)
        self.assertEqual(list(s)[:10],
            [-3, 1, 4, 4, 7, 9, 10, 13, 14, 16])

    def test_imerge2(self):
        a = range(10)
        b = range(1, 6)
        c = range(3, 14)
        a.reverse()
        b.reverse()
        c.reverse()
        self.assertEqual(list(iter.imerge(lambda a,b: a >= b, a, b, c)),
                         [13, 12,11, 10,
                          9, 9, 8, 8, 7, 7, 6, 6,
                          5, 5, 5, 4, 4, 4, 3, 3, 3,
                          2, 2, 1, 1,
                          0])

    def test_uniq(self):
        s = iter.uniq(iter.imerge(lambda x, y: x < y, self.s1, self.s2, self.s3))
        self.assertEqual(list(s)[:10],
            [-3, 1, 4, 7, 9, 10, 13, 14, 16, 19])


class Math(unittest.TestCase):
    def test_integral(self):
        self.assertTrue(m.is_integral(int(1)))
        self.assertTrue(m.is_integral(long(1)))
        self.assertFalse(m.is_integral("foo"))
        self.assertFalse(m.is_integral(1.0))
        self.assertFalse(m.is_integral(20 / 1))
        self.assertFalse(m.is_integral(Fraction(100, 10)))

    def test_lcm(self):
        self.assertEqual(m.lcm(), 0)
        self.assertEqual(m.lcm(99), 99)
        self.assertEqual(m.lcm(10, 20, 3), 60)
        self.assertEqual(m.lcm(10, 20), 20)
        self.assertEqual(m.lcm(3, 4), 12)

    def test_topsum(self):
        vals = [30, 60, 10, 40, 50, 20]
        self.assertEqual(m.topsum(vals, lambda x: x * 2, 3), 2 * (40 + 50 + 60))
        self.assertEqual(m.lcm(99), 99)
        self.assertEqual(m.lcm(10, 20, 3), 60)


class LinEqs(unittest.TestCase):
    def setUp(self):
        self.f     = m.lin(1, 3)
        self.c     = m.const(123)
        self.pwlin = m.monotonic_pwlin([(0, 1), (1, 0), (1, 4), (2, 5)])

    def test_const(self):
        for x in xrange(1000):
            self.assertAlmostEqual(self.c(x), 123)

    def test_lin(self):
        for x in xrange(1000):
            self.assertAlmostEqual(self.f(x), 1 + x * 3.0)

    def test_pwlin(self):
        for x in xrange(1000):
            self.assertAlmostEqual(self.pwlin(-x), 1)
        self.assertAlmostEqual(self.pwlin(1), 1)
        for x in xrange(1000):
            x = x + 2
            self.assertAlmostEqual(self.pwlin(x), x + 3)


