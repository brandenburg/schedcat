import unittest

from schedcat.util.time import ms2us

import schedcat.generator.tasks as tg
import schedcat.generator.tasksets as tsgen

class TaskGen(unittest.TestCase):

    def test_drawing_functions(self):
        f = tg.uniform_int(10, 100)
        self.assertTrue(type(f()) == int)
        self.assertTrue(10 <= f() <= 100)

        f = tg.uniform(10, 100)
        self.assertTrue(type(f()) == float)
        self.assertTrue(10 <= f() <= 100)

        f = tg.uniform_choice("abcdefg")
        self.assertTrue(type(f()) == str)
        self.assertTrue('a' <= f() <= 'g')

        f = tg.exponential(0.1, 0.7, 0.4)
        self.assertTrue(type(f()) == float)
        self.assertTrue(0.1 <= f() <= 0.7)

    def test_limiters(self):
        global counter
        counter = 0
        def inc():
            global counter
            counter += 10
            return counter
    
        trun = tg.truncate(15, 35)(inc)

        self.assertEqual(trun(), 15)
        self.assertEqual(counter, 10)
        
        counter = 0
        lim = tg.redraw(15, 35)(inc)
        self.assertEqual(lim(), 20)
        self.assertEqual(counter, 20)


    def test_generator(self):
        periods  = tg.uniform_int(10, 100)
        utils    = tg.exponential(0.1, 0.9, 0.3)
        g = tg.TaskGenerator(periods, utils)
        
        self.assertEqual(len(list(g.tasks(max_tasks = 10))), 10)        
        self.assertLessEqual(len(list(g.tasks(max_util = 10))), 100)

        ts1 = g.tasks(max_util = 10, squeeze = True, time_conversion=ms2us)
        ts2 = g.tasks(max_util = 10, squeeze = False, time_conversion=ms2us)

        self.assertAlmostEqual(sum([t.utilization() for t in ts1]), 10, places=2)
        self.assertNotEqual(sum([t.utilization() for t in ts2]), 10)

    def test_task_system_creation(self):
        periods  = tg.uniform_int(10, 100)
        utils    = tg.exponential(0.1, 0.9, 0.3)
        g = tg.TaskGenerator(periods, utils)
        
        self.assertEqual(len(g.make_task_set(max_tasks = 10)), 10)
        self.assertLessEqual(len((g.make_task_set(max_util = 10))), 100)

        ts1 = g.make_task_set(max_util = 10, squeeze = True, time_conversion=ms2us)
        ts2 = g.make_task_set(max_util = 10, squeeze = False, time_conversion=ms2us)

        self.assertAlmostEqual(ts1.utilization(), 10, places=2)
        # Not strictly impossible, but very unlikely
        self.assertNotEqual(ts2.utilization(), 10)

class TaskSetGen(unittest.TestCase):

    def test_feasible_tasks(self):
        for name in tsgen.ALL_DISTS:
            g = tsgen.ALL_DISTS[name]
            ts = g(time_conversion=ms2us, max_tasks=4)
            self.assertLessEqual(ts.utilization(), 4)
