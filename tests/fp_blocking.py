from __future__ import division

import unittest
import copy

import schedcat.model.tasks as tasks
import schedcat.model.resources as r

import schedcat.sched.fp.rta as rta
import schedcat.sched.fp as fp

from collections import defaultdict

import schedcat.locking.bounds as lb
import schedcat.locking.native as cpp
import schedcat.locking.partition as lp

import schedcat.util.linprog


def response_times_consistent(tasks):
    for t in tasks:
        if t.response_time != t.response_old:
            return False
    return True

def pfp_sched_test_msrp_inflate(taskset_in):
    ts = copy.deepcopy(taskset_in)
    partitions = defaultdict(tasks.TaskSystem)
    for t in ts:
        t.uninflated_cost = t.cost
        t.response_time = t.cost
        t.response_old = 0
        partitions[t.partition].append(t)

    while not response_times_consistent(ts):
        for t in ts:
            t.cost = t.uninflated_cost
            assert t.response_time >= t.response_old # monotonicity
            t.response_old = t.response_time
        lb.apply_task_fair_mutex_bounds(ts, 1, pi_aware=True)
        for part in partitions:
            if not fp.is_schedulable(1, partitions[part]):
                return (False, ts)
    return (True, ts)

def pfp_sched_test_msrp_ilp(taskset_in):
    ts = copy.deepcopy(taskset_in)
    partitions = defaultdict(tasks.TaskSystem)
    for t in ts:
        t.uninflated_cost = t.cost
        t.response_old = 0
        t.response_time = t.cost
        t.blocked = 0
        partitions[t.partition].append(t)

    while not response_times_consistent(ts):
        for t in ts:
            t.cost = t.uninflated_cost
            assert t.response_time >= t.response_old # monotonicity
            t.response_old = t.response_time
        lb.apply_pfp_lp_msrp_bounds(ts)
        for part in partitions:
            if not fp.is_schedulable(1, partitions[part]):
                return (False, ts)
    return (True, ts)


class PFPSpinlockInflationVsILP1(unittest.TestCase):
    def setUp(self):
        mscale = 100
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(17 * mscale,  100 * mscale),
                tasks.SporadicTask(67 * mscale,  340 * mscale),
                tasks.SporadicTask(27 * mscale,  150 * mscale),
            ])

        self.ts[0].partition = 0
        self.ts[1].partition = 1
        self.ts[2].partition = 0
        self.ts.assign_ids()

        r.initialize_resource_model(self.ts)
        lb.assign_fp_preemption_levels(self.ts)

        self.ts[0].resmodel[0].add_read_request(1 * mscale)
        self.ts[1].resmodel[0].add_read_request(1 * mscale)
        self.ts[2].resmodel[0].add_write_request(1 * mscale)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_ilp_dominance(self):
        "check that the ILP analysis is at least as accurate as inflation"
        (res_inf, ts_inf) = pfp_sched_test_msrp_inflate(self.ts)
        (res_ilp, ts_ilp) = pfp_sched_test_msrp_ilp(self.ts)

        self.assertTrue(res_inf)
        self.assertTrue(res_ilp)
        for i in range(3):
            self.assertGreaterEqual(ts_inf[i].response_time, ts_ilp[i].response_time)

        self.assertEqual(ts_ilp[0].response_time, 1900)
        self.assertEqual(ts_ilp[1].response_time, 6800)
        self.assertEqual(ts_ilp[2].response_time, 4500)

        self.assertEqual(ts_inf[0].response_time, 2000)
        self.assertEqual(ts_inf[1].response_time, 6800)
        self.assertEqual(ts_inf[2].response_time, 4600)


class PFPSpinlockInflationVsILP2(unittest.TestCase):
    def setUp(self):
        mscale = 100
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask( 17 * mscale,  100 * mscale),
                tasks.SporadicTask( 64 * mscale,  132 * mscale),
                tasks.SporadicTask( 67 * mscale,  340 * mscale),
                tasks.SporadicTask( 74 * mscale,  137 * mscale),
                tasks.SporadicTask( 31 * mscale,  249 * mscale),
                tasks.SporadicTask( 47 * mscale,  115 * mscale),
                tasks.SporadicTask( 27 * mscale,  150 * mscale),
                tasks.SporadicTask( 53 * mscale,  424 * mscale),
                tasks.SporadicTask(192 * mscale,  884 * mscale),
            ])

        self.ts[0].partition = 0
        self.ts[1].partition = 1
        self.ts[2].partition = 2
        self.ts[3].partition = 3
        self.ts[4].partition = 4

        self.ts[5].partition = 0
        self.ts[6].partition = 0

        self.ts[7].partition = 2
        self.ts[8].partition = 4
        self.ts.assign_ids()

        r.initialize_resource_model(self.ts)
        lb.assign_fp_preemption_levels(self.ts)

        self.ts[0].resmodel[0].add_read_request(1 * mscale)
        self.ts[1].resmodel[0].add_read_request(1 * mscale)
        self.ts[2].resmodel[0].add_read_request(1 * mscale)
        self.ts[3].resmodel[0].add_read_request(1 * mscale)
        self.ts[4].resmodel[0].add_read_request(1 * mscale)

        self.ts[6].resmodel[0].add_write_request(1 * mscale)
        self.ts[7].resmodel[0].add_write_request(1 * mscale)
        self.ts[8].resmodel[0].add_write_request(1 * mscale)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_ilp_dominance(self):
        "check that the ILP analysis is at least as accurate as inflation"
        (res_inf, ts_inf) = pfp_sched_test_msrp_inflate(self.ts)
        (res_ilp, ts_ilp) = pfp_sched_test_msrp_ilp(self.ts)

        self.assertTrue(res_inf)
        self.assertTrue(res_ilp)
        for i in range(len(self.ts)):
            self.assertGreaterEqual(ts_inf[i].response_time, ts_ilp[i].response_time)

