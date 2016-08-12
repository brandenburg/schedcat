from __future__ import division

from fractions import Fraction

import unittest

import schedcat.sched.run as run

class RUNOfflinePhase(unittest.TestCase):
    def setUp(self):
        self.servers = [
                run.Server(Fraction(1, 2), id=1),
                run.Server(Fraction(4, 10), id=2),
                run.Server(Fraction(4, 10), id=3),
                run.Server(Fraction(3, 10), id=4),
                run.Server(Fraction(2, 10), id=5),
                run.Server(Fraction(1, 10), id=6),
                run.Server(Fraction(1, 10), id=7),
            ]

        self.example_packing = [
            [self.servers[0], self.servers[3]],
            [self.servers[1], self.servers[4], self.servers[5], self.servers[6]],
            [self.servers[2]]
        ]

    def test_find_packing(self):
        packing = run.find_packing(self.servers)
        all    = set(self.servers)
        packed = set()
        for bin in packing:
            for s in bin:
                packed.add(s)
        self.assertEqual(all, packed)

    def test_pack(self):
        packed = run.pack(self.example_packing, 8)
        self.assertEqual(len(packed), len(self.example_packing))
        for s, bin in zip(packed, self.example_packing):
            self.assertEqual(s.clients, bin)

    def test_dual(self):
        packed = run.pack(self.example_packing, 8)
        duals  = run.dual(packed)
        self.assertEqual(len(duals), len(self.example_packing))
        for d, s in zip(duals, packed):
            self.assertEqual(1, d.rate + s.rate)

        self.assertFalse(duals[0].is_null_server())
        self.assertFalse(duals[1].is_null_server())
        self.assertFalse(duals[2].is_null_server())
        self.assertFalse(duals[0].is_unit_server())
        self.assertFalse(duals[1].is_unit_server())
        self.assertFalse(duals[2].is_unit_server())

    def test_reduction_step(self):
        red = run.reduction_step(self.servers, 8)
        self.assertTrue(red[0].is_null_server())
        self.assertTrue(red[1].is_null_server())
        self.assertFalse(red[0].is_unit_server())
        self.assertFalse(red[1].is_unit_server())

    def test_reduce(self):
        final, levels = run.reduce(self.servers, 8)
        for s in final:
            self.assertEqual(s.rate, 1)
        self.assertEqual(len(levels), 1)
        self.assertEqual(len(final), 2)
        self.assertEqual(run.max_number_of_preemptions_per_job_release(levels), 1)

    def test_reduce_non_integer(self):
        self.servers.append(run.Server(Fraction(1, 3), id='frac'))
        final, levels = run.reduce(self.servers, 8)
        for s in final:
            self.assertEqual(s.rate, 1)
        self.assertEqual(len(final), 3)
        self.assertEqual(len(levels), 1)
        self.assertEqual(run.max_number_of_preemptions_per_job_release(levels), 1)
