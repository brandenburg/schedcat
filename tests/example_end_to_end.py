# Uses the example code in the example/ directory, since it's a good end-to-end
# test.
from __future__ import division

import unittest

from example.driver import get_script_dir, nolock_example, lock_example

nolock_example_1_list = [0, 55575, 55575, 67570, 67569, 8805, 8805, 68149,
                         68149, 12514, 12513, 55400, 55400, 76501, 76501,
                         66761, 66762, -324, -324, 91995, 91995, 35996, 35997]

nolock_example_2_list = None
lock_example_1_list = [-44967, -104360, -9607, -44134, -53237, -31791, -68420,
                       -67526, -55107, -47183, -11898]

lock_example_2_list = None

class NoLockExample(unittest.TestCase):
    def setUp(self):
        example_dir = get_script_dir()
        self.nolock_example_1 = example_dir + "/nolock_example_1"
        self.nolock_example_2 = example_dir + "/nolock_example_2"
        results = nolock_example([self.nolock_example_1, self.nolock_example_2])
        self.distilled = []
        for (name, clusts) in results:
            if clusts is None:
                self.distilled.append((name, None))
            else:
                new_distilled = []
                for clust in clusts:
                    new_distilled += [task.response_time - task.deadline
                                     for task in clust]
                self.distilled.append((name, new_distilled))

    def test_nolock_example_1_works(self):
        self.assertEqual(self.distilled[0], (self.nolock_example_1,
                                             nolock_example_1_list))

    def test_nolock_example_2_works(self):
        self.assertEqual(self.distilled[1], (self.nolock_example_2,
                                             nolock_example_2_list))

class LockExample(unittest.TestCase):
    def setUp(self):
        example_dir = get_script_dir()
        self.lock_example_1 = example_dir + "/lock_example_1"
        self.lock_example_2 = example_dir + "/lock_example_2"
        results = lock_example([self.lock_example_1, self.lock_example_2])
        self.distilled = []
        for (name, clusts) in results:
            if clusts is None:
                self.distilled.append((name, None))
            else:
                new_distilled = []
                for clust in clusts:
                    new_distilled += [task.response_time - task.deadline
                                     for task in clust]
                self.distilled.append((name, new_distilled))

    def test_nolock_example_1_works(self):
        self.assertEqual(self.distilled[0], (self.lock_example_1,
                                             lock_example_1_list))

    def test_nolock_example_2_works(self):
        self.assertEqual(self.distilled[1], (self.lock_example_2,
                                             lock_example_2_list))
