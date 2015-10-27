from __future__ import division

import unittest
import random

import schedcat.locking.bounds as lb
import schedcat.locking.native as cpp
import schedcat.model.tasks as tasks
import schedcat.model.resources as r

import schedcat.util.linprog

class GlobalAnalysis(unittest.TestCase):
    def setUp(self):
        self.trivial_ts = tasks.TaskSystem([
                tasks.SporadicTask(25,  150),
                tasks.SporadicTask(30,  200),
                tasks.SporadicTask(60,  800),
                tasks.SporadicTask(30,  550),
                tasks.SporadicTask(200, 1000),
                tasks.SporadicTask(100, 825),
                tasks.SporadicTask(50, 1100),
            ])
        self.trivial_num_cpus = 2

        r.initialize_resource_model(self.trivial_ts)
        lb.assign_fp_preemption_levels(self.trivial_ts)

        for i, t in enumerate(self.trivial_ts):
            t.partition = 0
            t.response_time = 4*t.cost


        self.trivial_ts[0].resmodel[1].add_request(5)
        self.trivial_ts[0].resmodel[1].add_request(5)
        self.trivial_ts[0].resmodel[1].add_request(5)

        self.trivial_ts[1].resmodel[2].add_request(10)
        self.trivial_ts[1].resmodel[3].add_request(10)

        self.trivial_ts[2].resmodel[1].add_request(10)
        self.trivial_ts[2].resmodel[2].add_request(5)

        self.trivial_ts[3].resmodel[1].add_request(15)

        self.trivial_ts[4].resmodel[2].add_request(10)
        self.trivial_ts[4].resmodel[3].add_request(10)
        self.trivial_ts[4].resmodel[1].add_request(5)

        self.trivial_ts[5].resmodel[4].add_request(8)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_no_progress_priority_trivial(self):
        lb.apply_no_progress_priority_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 75)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 90)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 115)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 100)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 280)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_no_progress_fifo_trivial(self):
        lb.apply_no_progress_fifo_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 75)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 92)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 107)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 87)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 267)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_prsb_trivial(self):
        lb.apply_prsb_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 93)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 100)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 155)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 118)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 300)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_global_fmlpp_trivial(self):
        lb.apply_global_fmlpp_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 93)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 103)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 130)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 105)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 275)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_ppcp_trivial(self):
        lb.apply_ppcp_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 30)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 20)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 160)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 120)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 292)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_gfmlp_trivial(self):
        lb.apply_sa_gfmlp_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 30)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 25)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 117)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 97)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 267)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pip_trivial(self):
        lb.apply_pip_bounds(self.trivial_ts, self.trivial_num_cpus)

        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 30)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 20)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 127)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 110)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 280)
        self.assertEqual(self.trivial_ts[5].response_time, self.trivial_ts[5].cost + 240)
        self.assertEqual(self.trivial_ts[6].response_time, self.trivial_ts[6].cost + 250)


class GlobalPPCPAnalysis(unittest.TestCase):
    def setUp(self):
        self.trivial_ts = tasks.TaskSystem([
                tasks.SporadicTask(20,  200),
                tasks.SporadicTask(20,  200),
                tasks.SporadicTask(50,  500),
                tasks.SporadicTask(20,  400),
                tasks.SporadicTask(100, 800),
            ])
        self.trivial_num_cpus = 2

        r.initialize_resource_model(self.trivial_ts)
        lb.assign_fp_preemption_levels(self.trivial_ts)

        for i, t in enumerate(self.trivial_ts):
            t.partition = 0
            t.response_time = 4*t.cost


        self.trivial_ts[0].resmodel[1].add_request(5)
        self.trivial_ts[0].resmodel[1].add_request(5)
        self.trivial_ts[1].resmodel[2].add_request(5)
        self.trivial_ts[1].resmodel[2].add_request(5)
        self.trivial_ts[2].resmodel[1].add_request(10)
        self.trivial_ts[4].resmodel[2].add_request(10)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_ppcp_trivial(self):
        lb.apply_ppcp_bounds(self.trivial_ts, self.trivial_num_cpus)
        self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 10)
        self.assertEqual(self.trivial_ts[1].response_time, self.trivial_ts[1].cost + 10)
        self.assertEqual(self.trivial_ts[2].response_time, self.trivial_ts[2].cost + 60)
        self.assertEqual(self.trivial_ts[3].response_time, self.trivial_ts[3].cost + 50)
        self.assertEqual(self.trivial_ts[4].response_time, self.trivial_ts[4].cost + 135)

