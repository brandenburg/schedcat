#!/usr/bin/env python2

from __future__ import division

import unittest

import schedcat.mapping.binpack as bp
import schedcat.mapping.rollback as rb

class TooLarge(unittest.TestCase):
    def setUp(self):
        self.cap   = 10
        self.items = range(100, 1000)
        self.bins  = 9
        self.empty = [[]] * self.bins

    def test_next_fit(self):
        sets = bp.next_fit(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_first_fit(self):
        sets = bp.first_fit(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_worst_fit(self):
        sets = bp.worst_fit(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_best_fit(self):
        sets = bp.best_fit(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_next_fit_decreasing(self):
        sets = bp.next_fit_decreasing(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_first_fit_decreasing(self):
        sets = bp.first_fit_decreasing(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_worst_fit_decreasing(self):
        sets = bp.worst_fit_decreasing(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

    def test_best_fit_decreasing(self):
        sets = bp.best_fit_decreasing(self.items, self.bins, self.cap)
        self.assertEqual(sets, self.empty)

class NotLossy(unittest.TestCase):
    def setUp(self):
        self.items = range(100, 1000)
        self.bins  = 1
        self.cap   = sum(self.items)
        self.expected = [self.items]

    def test_next_fit(self):
        sets = bp.next_fit(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_first_fit(self):
        sets = bp.first_fit(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_worst_fit(self):
        sets = bp.worst_fit(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_best_fit(self):
        sets = bp.best_fit(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_next_fit_decreasing(self):
        sets = bp.next_fit_decreasing(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_first_fit_decreasing(self):
        sets = bp.first_fit_decreasing(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_worst_fit_decreasing(self):
        sets = bp.worst_fit_decreasing(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)

    def test_best_fit_decreasing(self):
        sets = bp.best_fit_decreasing(self.items, self.bins, self.cap)
        sets[0].sort()
        self.assertEqual(sets, self.expected)


class KnownExample(unittest.TestCase):
    def setUp(self):
        self.items = [8, 5, 7, 6, 2, 4, 1]
        self.bins  = 5
        self.cap   = 10

    def test_next_fit(self):
        sets = bp.next_fit(self.items, self.bins, self.cap)
        self.expected = [[8], [5], [7], [6, 2], [4, 1]]
        self.assertEqual(sets, self.expected)

    def test_first_fit(self):
        sets = bp.first_fit(self.items, self.bins, self.cap)
        self.expected = [[8, 2], [5, 4, 1], [7], [6], []]
        self.assertEqual(sets, self.expected)

    def test_worst_fit(self):
        self.bins = 4
        sets = bp.worst_fit(self.items, self.bins, self.cap)
        self.expected = [[8], [5, 2, 1], [7], [6, 4]]
        self.assertEqual(sets, self.expected)

    def test_best_fit(self):
        sets = bp.best_fit(self.items, self.bins, self.cap)
        self.expected = [[8, 2], [5], [7, 1], [6, 4], []]
        self.assertEqual(sets, self.expected)


class RollbackBins(unittest.TestCase):
    def setUp(self):
        self.bin = rb.Bin([0.2])
        self.bin2 = rb.Bin([2], size=lambda x: x + 0.1, capacity=10)

    def test_try_to_add_float(self):
        self.assertTrue(self.bin.try_to_add(0.3))
        self.assertFalse(self.bin.try_to_add(0.6))
        self.assertTrue(self.bin.try_to_add(0.5))
        self.assertFalse(self.bin.try_to_add(0.0000001))
        self.assertEqual([0.2,0.3,0.5], self.bin.items)

    def test_try_to_add_int(self):
        self.assertTrue(self.bin2.try_to_add(3))
        self.assertFalse(self.bin2.try_to_add(6))
        self.assertFalse(self.bin2.try_to_add(5))
        self.assertTrue(self.bin2.try_to_add(1))
        self.assertEqual([2,3,1], self.bin2.items)


class Heuristics(unittest.TestCase):
    def setUp(self):
        self.items = [8, 5, 7, 6, 2, 4, 1]
        self.bins  = [rb.Bin(capacity = 10) for _ in range(5)]
        self.bins  = [rb.CheckedBin(b) for b in self.bins]
        self.make_bin = lambda x: rb.CheckedBin(rb.Bin(capacity = x))

    def test_base(self):
        h = rb.Heuristic(self.bins)
        self.assertEqual(0, h.binpack(self.items))
        self.assertEqual(h.misfits, self.items)
        h.misfits = []
        self.assertRaises(bp.DidNotFit, h.binpack, self.items,
                          report_misfit=bp.report_failure)
        self.assertEqual([8], h.misfits)

    def test_next_fit_fixed(self):
        h = rb.NextFit(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5], [7], [6, 2], [4, 1]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_next_fit_make(self):
        h = rb.NextFit(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5], [7], [6, 2], [4, 1]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_next_fit_small(self):
        h = rb.NextFit(make_bin=lambda: self.make_bin(7))
        expected = [[5], [7], [6], [2, 4, 1]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_next_fit_few(self):
        h = rb.NextFit(self.bins[:4])
        expected = [[8], [5], [7], [6, 2]]
        misfits  = [4, 1]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)


    def test_first_fit_fixed(self):
        h = rb.FirstFit(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5, 4, 1], [7], [6], []]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_first_fit_make(self):
        h = rb.FirstFit(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5, 4, 1], [7], [6]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_first_fit_small(self):
        h = rb.FirstFit(make_bin=lambda: self.make_bin(7))
        expected = [[5,2], [7], [6, 1], [4]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_first_fit_few(self):
        h = rb.FirstFit(self.bins[:3])
        expected = [[8, 2], [5, 4, 1], [7]]
        misfits  = [6]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)


    def test_worst_fit_fixed(self):
        h = rb.WorstFit(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5, 1], [7], [6], [2, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_worst_fit_make(self):
        h = rb.WorstFit(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5, 2, 1], [7], [6, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_worst_fit_small(self):
        h = rb.WorstFit(make_bin=lambda: self.make_bin(7))
        expected = [[5, 2], [7], [6], [4, 1]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_worst_fit_few(self):
        h = rb.WorstFit(self.bins[:4])
        expected = [[8], [5, 2, 1], [7], [6, 4]]
        misfits  = []
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)


    def test_best_fit_fixed(self):
        h = rb.BestFit(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5], [7, 1], [6, 4], []]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_best_fit_make(self):
        h = rb.BestFit(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5], [7, 1], [6, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_best_fit_small(self):
        h = rb.BestFit(make_bin=lambda: self.make_bin(7))
        expected = [[5, 2], [7], [6, 1], [4]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_best_fit_few(self):
        h = rb.BestFit(self.bins[:3])
        expected = [[8, 2], [5, 4, 1], [7]]
        misfits  = [6]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)


    def test_max_spare_cap_fixed(self):
        h = rb.MaxSpareCapacity(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5, 1], [7], [6], [2, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_max_spare_cap_make(self):
        h = rb.MaxSpareCapacity(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8], [5, 2, 1], [7], [6, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_max_spare_cap_small(self):
        h = rb.MaxSpareCapacity(make_bin=lambda: self.make_bin(7))
        expected = [[5, 2], [7], [6], [4, 1]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_max_spare_cap_few(self):
        h = rb.MaxSpareCapacity(self.bins[:4])
        expected = [[8], [5, 2, 1], [7], [6, 4]]
        misfits  = []
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)


    def test_min_spare_cap_fixed(self):
        h = rb.MinSpareCapacity(self.bins)
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5], [7, 1], [6, 4], []]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_min_spare_cap_make(self):
        h = rb.MinSpareCapacity(make_bin=lambda: self.make_bin(10))
        self.assertEqual(len(self.items), h.binpack(self.items))
        expected = [[8, 2], [5], [7, 1], [6, 4]]
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)

    def test_min_spare_cap_small(self):
        h = rb.MinSpareCapacity(make_bin=lambda: self.make_bin(7))
        expected = [[5, 2], [7], [6, 1], [4]]
        misfits  = [8]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)

    def test_min_spare_cap_few(self):
        h = rb.MinSpareCapacity(self.bins[:3])
        expected = [[8, 2], [5, 4, 1], [7]]
        misfits  = [6]
        self.assertEqual(len(self.items) - len(misfits),
                         h.binpack(self.items))
        did_part = [bin.items for bin in h.bins]
        self.assertEqual(expected, did_part)
        self.assertEqual(misfits, h.misfits)
