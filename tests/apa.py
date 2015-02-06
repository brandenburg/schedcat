#!/usr/bin/env python

from __future__ import division

import unittest

import schedcat.mapping.binpack as bp
import schedcat.mapping.apa as apa
import schedcat.model.tasks as tasks

class PartitioningHeuristics(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
            tasks.SporadicTask(10, 100),
            tasks.SporadicTask(30, 100),
            tasks.SporadicTask(20, 100, deadline=30),
            tasks.SporadicTask(21, 30),
            ])

        self.ts[0].affinity = set([1])
        self.ts[1].affinity = set([1, 2])
        self.ts[2].affinity = set([2, 3])
        self.ts[3].affinity = set([1, 2, 3])

        self.cores = set([1, 2, 3])

    def test_sorted_by_decreasing_difficulty(self):
        got = apa.sorted_by_decreasing_difficulty(self.ts)
        expected = [
            self.ts[0],
            self.ts[2],
            self.ts[1],
            self.ts[3],
        ]
        self.assertEqual(expected, got)

        got = apa.sorted_by_decreasing_difficulty(
            [t for t in self.ts if 2 in t.affinity])
        expected = [
            self.ts[2],
            self.ts[1],
            self.ts[3],
        ]
        self.assertEqual(expected, got)


    def test_first_fit(self):
        failed, mapping = apa.edf_first_fit_decreasing_difficulty(self.ts, [1, 2, 3])

        self.assertEqual(failed, set())
        self.assertEqual(len(mapping), 3)
        self.assertEqual(mapping[1], [self.ts[0], self.ts[1]])
        self.assertEqual(mapping[2], [self.ts[3]])
        self.assertEqual(mapping[3], [self.ts[2]])


        failed, mapping = apa.edf_first_fit_decreasing_difficulty(self.ts, [2, 1, 3])

        self.assertEqual(failed, set())
        self.assertEqual(len(mapping), 3)
        self.assertEqual(mapping[2], [self.ts[2], self.ts[1]])
        self.assertEqual(mapping[1], [self.ts[0], self.ts[3]])
        self.assertEqual(mapping[3], [])


        # give it one more to place
        self.ts.append(tasks.SporadicTask(19, 31))
        self.ts[-1].affinity = set([2, 3])

        failed, mapping = apa.edf_first_fit_decreasing_difficulty(self.ts, [1, 2, 3])
        self.assertEqual(failed, set([self.ts[-1]]))

        failed, mapping = apa.edf_first_fit_decreasing_difficulty(self.ts, [2, 1, 3])

        self.assertEqual(failed, set())
        self.assertEqual(len(mapping), 3)
        self.assertEqual(mapping[2], [self.ts[2], self.ts[1]])
        self.assertEqual(mapping[1], [self.ts[0], self.ts[3]])
        self.assertEqual(mapping[3], [self.ts[4]])
