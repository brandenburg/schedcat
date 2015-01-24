from __future__ import division

import unittest
import random

import schedcat.locking.bounds as lb
import schedcat.locking.native as cpp
import schedcat.locking.partition as lp
import schedcat.model.tasks as tasks
import schedcat.model.resources as r

import schedcat.util.linprog

class Locking(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(3, 18),
            ])
        r.initialize_resource_model(self.ts)
        for i, t in enumerate(self.ts):
            t.partition = 0
            t.response_time = t.period
            t.resmodel[1].add_request(1)

    def test_fp_preemption_levels(self):
        self.ts.sort_by_period()
        lb.assign_fp_preemption_levels(self.ts)
        self.assertEqual(self.ts[0].preemption_level, 0)
        self.assertEqual(self.ts[1].preemption_level, 1)
        self.assertEqual(self.ts[2].preemption_level, 2)
        self.assertEqual(self.ts[3].preemption_level, 3)

    def test_edf_preemption_levels(self):
        self.ts[0].deadline = 5
        self.ts[3].deadline = 9
        ts = list(self.ts)
        random.shuffle(ts)
        lb.assign_edf_preemption_levels(self.ts)
        self.assertEqual(self.ts[0].preemption_level, 0)
        self.assertEqual(self.ts[1].preemption_level, 0)
        self.assertEqual(self.ts[2].preemption_level, 1)
        self.assertEqual(self.ts[3].preemption_level, 1)


    def test_cpp_bridge(self):
        lb.assign_fp_preemption_levels(self.ts)
        self.assertIsNotNone(lb.get_cpp_model(self.ts))
        self.assertIsNotNone(lb.get_cpp_model_rw(self.ts))


class ApplyBounds(unittest.TestCase):
# This primarily checks that the tests don't crash.
# TODO: add actual tests of the computed bounds.
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(3, 18),
            ])
        self.ts_ = self.ts.copy()
        r.initialize_resource_model(self.ts)
        for i, t in enumerate(self.ts):
            t.partition = i % 2
            t.response_time = t.period
            t.resmodel[0].add_request(1)
            t.resmodel[1].add_request(1)
        lb.assign_fp_preemption_levels(self.ts)

    def saw_non_zero_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertGreater(t.suspended, 0)
            self.assertGreater(t.blocked, 0)
            self.assertEqual(t.cost, t_.cost)
            self.assertEqual(t.period, t_.period)

    def saw_only_local_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertEqual(t.suspended, 0)
            self.assertEqual(t.period, t_.period)

    def sob_non_zero_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertGreater(t.sob_blocked, 0)
            self.assertEqual(t.suspended, 0)
            self.assertEqual(t.blocked, 0)
            self.assertGreater(t.cost, t_.cost)
            self.assertEqual(t.period, t_.period)

    def lp_non_zero_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertGreater(t.blocked, 0)
            self.assertEqual(t.cost, t_.cost)
            self.assertEqual(t.period, t_.period)

    def lp_zero_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertEqual(t.blocked, 0)
            self.assertEqual(t.cost, t_.cost)
            self.assertEqual(t.period, t_.period)


    def test_mpcp(self):
        lb.apply_mpcp_bounds(self.ts, use_virtual_spin=False)
        self.saw_non_zero_blocking()

    def test_mpcpvs(self):
        lb.apply_mpcp_bounds(self.ts, use_virtual_spin=True)
        self.saw_only_local_blocking()

    def test_dpcp(self):
        rmap = lb.get_round_robin_resource_mapping(2, 2)
        lb.apply_dpcp_bounds(self.ts, rmap)
        self.saw_non_zero_blocking()

    def test_part_fmlp(self):
        lb.apply_part_fmlp_bounds(self.ts, preemptive=True)
        self.saw_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_generalized_fmlp1(self):
        lb.apply_generalized_fmlp_bounds(self.ts, 1, True)
        self.saw_non_zero_blocking()

        lb.apply_generalized_fmlp_bounds(self.ts, 1, False)
        self.saw_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_generalized_fmlp2(self):
        lb.apply_generalized_fmlp_bounds(self.ts, 2, True)
        self.saw_non_zero_blocking()

        lb.apply_generalized_fmlp_bounds(self.ts, 2, False)
        self.saw_non_zero_blocking()

    def test_part_fmlp_np(self):
        lb.apply_part_fmlp_bounds(self.ts, preemptive=False)
        self.saw_non_zero_blocking()

    def test_global_fmlp(self):
        lb.apply_global_fmlp_sob_bounds(self.ts)
        self.sob_non_zero_blocking()

    def test_global_omlp(self):
        lb.apply_global_omlp_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_omip(self):
        lb.apply_omip_bounds(self.ts, 2, 1)
        self.sob_non_zero_blocking()

    def test_clustered_omlp(self):
        lb.apply_clustered_omlp_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    def test_clustered_rw_omlp(self):
        lb.apply_clustered_rw_omlp_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    def test_clustered_kx_omlp(self):
        lb.apply_clustered_kx_omlp_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    def test_clustered_kx_omlp_no_replication(self):
        ts = self.ts.copy()
        lb.apply_clustered_kx_omlp_bounds(self.ts, 2)
        lb.apply_clustered_omlp_bounds(ts, 2)
        self.assertEqual(self.ts[0].cost, ts[0].cost)
        self.assertEqual(self.ts[1].cost, ts[1].cost)
        self.assertEqual(self.ts[2].cost, ts[2].cost)
        self.assertEqual(self.ts[3].cost, ts[3].cost)

    def test_clustered_kx_omlp_bounds(self):
        replicas = {0:2, 1:4}
        lb.apply_clustered_kx_omlp_bounds(self.ts, 2, replicas)
        self.assertEqual(self.ts[0].cost, 1 + 1 + 0 + 2)
        self.assertEqual(self.ts[1].cost, 1 + 1 + 0 + 2)
        self.assertEqual(self.ts[2].cost, 3 + 1 + 0)
        self.assertEqual(self.ts[3].cost, 3 + 1 + 0)

    def test_clustered_kx_omlp_donation_blocking(self):
        replicas = {0:4, 1:4}
        lb.apply_clustered_kx_omlp_bounds(self.ts, 2, replicas)
        self.assertEqual(self.ts[0].cost, 1 + 0 + 0 + 1)
        self.assertEqual(self.ts[1].cost, 1 + 0 + 0 + 1)
        self.assertEqual(self.ts[2].cost, 3 + 0)
        self.assertEqual(self.ts[3].cost, 3 + 0)

    def test_tfmtx(self):
        lb.apply_task_fair_mutex_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    def test_tfrw(self):
        lb.apply_task_fair_rw_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    def test_pfrw(self):
        lb.apply_phase_fair_rw_bounds(self.ts, 2)
        self.sob_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_preemptive_fifo_bounds(self):
        lb.apply_pfp_lp_preemptive_fifo_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_msrp_bounds(self):
        lb.apply_pfp_lp_msrp_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_unordered_bounds(self):
        lb.apply_pfp_lp_unordered_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_preemptive_unordered_bounds(self):
        lb.apply_pfp_lp_preemptive_unordered_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_prio_bounds(self):
        lb.apply_pfp_lp_prio_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_preemptive_prio_bounds(self):
        lb.apply_pfp_lp_preemptive_prio_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_prio_fifo_bounds(self):
        lb.apply_pfp_lp_prio_fifo_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_pfp_lp_preemptive_prio_fifo_bounds(self):
        lb.apply_pfp_lp_preemptive_prio_fifo_bounds(self.ts)
        self.lp_non_zero_blocking()

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dummy_bounds(self):
        lb.apply_dummy_bounds(self.ts)
        self.lp_zero_blocking()


# lower-level tests for C++ implementation

class Test_bounds(unittest.TestCase):

    def setUp(self):
        self.rsi1 = cpp.ResourceSharingInfo(6)

        self.rsi1.add_task(100, 100, 0, 0)
        self.rsi1.add_request(0, 1, 77)

        self.rsi1.add_task(50, 50, 0, 1)
        self.rsi1.add_request(0, 1, 6)

        self.rsi1.add_task(10, 10, 0, 2)
        self.rsi1.add_request(0, 1, 5)

        self.rsi1.add_task(10, 10, 1, 3)
        self.rsi1.add_request(0, 1, 7)

        self.rsi1.add_task(20, 20, 2, 4)
        self.rsi1.add_request(0, 4, 1)

        self.rsi1.add_task(30, 30, 2, 5)
        self.rsi1.add_request(0, 1, 7)


    def test_arrival_blocking_mtx(self):

        c = 1

        res = cpp.task_fair_mutex_bounds(self.rsi1, c)
        self.assertEqual(6 + 7 + 7, res.get_arrival_blocking(0))
        self.assertEqual(5 + 7 + 7, res.get_arrival_blocking(1))
        self.assertEqual(0, res.get_arrival_blocking(2))
        self.assertEqual(0, res.get_arrival_blocking(3))
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))
        self.assertEqual(0, res.get_arrival_blocking(5))


    def test_arrival_blocking_msrp(self):
        res = cpp.msrp_bounds_holistic(self.rsi1)
        self.assertEqual(6 + 7 + 7, res.get_arrival_blocking(0))
        self.assertEqual(5 + 7 + 7, res.get_arrival_blocking(1))
        self.assertEqual(0, res.get_arrival_blocking(2))
        self.assertEqual(0, res.get_arrival_blocking(3))
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))
        self.assertEqual(0, res.get_arrival_blocking(5))

    def test_local_resources_msrp(self):
        self.rsi1.add_request(99, 100, 123456)
        res = cpp.msrp_bounds_holistic(self.rsi1)
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))

    def test_local_resources_msrp2(self):
        self.rsi1 = cpp.ResourceSharingInfo(7)

        self.rsi1.add_task(100, 100, 0, 0)
        self.rsi1.add_request(0, 1, 77)

        self.rsi1.add_task(50, 50, 0, 1)
        self.rsi1.add_request(0, 1, 6)

        self.rsi1.add_task(10, 10, 0, 2)
        self.rsi1.add_request(0, 1, 5)

        self.rsi1.add_task(10, 10, 1, 3)
        self.rsi1.add_request(0, 1, 7)

        self.rsi1.add_task(20, 20, 2, 4)
        self.rsi1.add_request(0, 4, 1)

        self.rsi1.add_task(30, 30, 2, 5)
        self.rsi1.add_request(0, 1, 7)
        self.rsi1.add_request(99, 1, 1)

        self.rsi1.add_task(100, 100, 2, 6)
        self.rsi1.add_request(99, 100, 123456)
        res = cpp.msrp_bounds_holistic(self.rsi1)
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))
        self.assertEqual(3 * 7 + 2 * 77 + 2 * 6, res.get_remote_blocking(4))
        self.assertEqual(123456, res.get_arrival_blocking(5))
        self.assertEqual(123456 + 7 + 77, res.get_blocking_term(5))
        self.assertEqual(7 + 77, res.get_remote_blocking(5))
        self.assertEqual(0, res.get_blocking_term(6))
        self.assertEqual(0, res.get_arrival_blocking(6))

    def test_arrival_blocking_pf(self):
        c = 1

        res = cpp.phase_fair_rw_bounds(self.rsi1, c)
        self.assertEqual(6 + 7 + 7, res.get_arrival_blocking(0))
        self.assertEqual(5 + 7 + 7, res.get_arrival_blocking(1))
        self.assertEqual(0, res.get_arrival_blocking(2))
        self.assertEqual(0, res.get_arrival_blocking(3))
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))
        self.assertEqual(0, res.get_arrival_blocking(5))

    def test_arrival_blocking_tf(self):
        c = 1

        res = cpp.task_fair_rw_bounds(self.rsi1, self.rsi1, c)
        self.assertEqual(6 + 7 + 7, res.get_arrival_blocking(0))
        self.assertEqual(5 + 7 + 7, res.get_arrival_blocking(1))
        self.assertEqual(0, res.get_arrival_blocking(2))
        self.assertEqual(0, res.get_arrival_blocking(3))
        self.assertEqual(7 + 7 + 77, res.get_arrival_blocking(4))
        self.assertEqual(0, res.get_arrival_blocking(5))



class Test_dedicated_irq(unittest.TestCase):

    def setUp(self):
        self.rsi = cpp.ResourceSharingInfo(16)

        idx = 0
        for cluster, length in zip(range(4), [1, 3, 5, 7]):
            for _ in range(4):
                self.rsi.add_task(100, 100, cluster, idx)
                self.rsi.add_request(0, 1, length)
                idx += 1

        self.rsi_rw = cpp.ResourceSharingInfo(16)

        idx = 0
        for cluster, length in zip(range(4), [1, 3, 5, 7]):
            for _ in range(4):
                self.rsi_rw.add_task(100, 100, cluster, idx)
                self.rsi_rw.add_request_rw(0, 1, length,
                                           cpp.READ if cluster > 0 else cpp.WRITE)
                idx += 1

    def test_global_irq_blocking(self):
        cluster_size = 2
        dedicated_cpu = cpp.NO_CPU
        res = cpp.task_fair_mutex_bounds(self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(7, res.get_blocking_count(15))
        self.assertEqual(2 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 2 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(15, res.get_blocking_count(0))
        self.assertEqual(arrival + 1 + 6 + 10 + 14, res.get_blocking_term(0))

    def test_dedicated_irq_blocking(self):
        cluster_size = 2
        dedicated_cpu = 0
        res = cpp.task_fair_mutex_bounds(self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(6, res.get_blocking_count(15))
        self.assertEqual(1 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 1 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(7 + 6, res.get_blocking_count(0))
        self.assertEqual(arrival + 6 + 10 + 14, res.get_blocking_term(0))


    def test_global_irq_tfrw_blocking(self):
        cluster_size = 2
        dedicated_cpu = cpp.NO_CPU

        res = cpp.task_fair_rw_bounds(self.rsi, self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(7, res.get_blocking_count(15))
        self.assertEqual(2 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 2 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(15, res.get_blocking_count(0))
        self.assertEqual(arrival + 1 + 6 + 10 + 14, res.get_blocking_term(0))



        res = cpp.task_fair_rw_bounds(self.rsi_rw, self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(4, res.get_blocking_count(15))
        self.assertEqual(2 + 5 + 7, res.get_blocking_term(15))

        arrival = 2 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(7, res.get_blocking_count(0))
        # pessimism
        self.assertEqual(arrival + 1 + 14, res.get_blocking_term(0))



    def test_dedicated_irq_tfrw_blocking(self):
        cluster_size = 2
        dedicated_cpu = 0
        res = cpp.task_fair_rw_bounds(self.rsi, self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(6, res.get_blocking_count(15))
        self.assertEqual(1 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 1 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(7 + 6, res.get_blocking_count(0))
        self.assertEqual(arrival + 6 + 10 + 14, res.get_blocking_term(0))



        res = cpp.task_fair_rw_bounds(self.rsi_rw, self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(2, res.get_blocking_count(15))
        self.assertEqual(1 + 7, res.get_blocking_term(15))

        arrival = 1 + 7
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(3, res.get_blocking_count(0))
        # pessimism
        self.assertEqual(arrival + 7, res.get_blocking_term(0))



    def test_global_irq_pfrw_blocking(self):
        cluster_size = 2
        dedicated_cpu = cpp.NO_CPU
        res = cpp.phase_fair_rw_bounds(self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(7, res.get_blocking_count(15))
        self.assertEqual(2 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 2 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(15, res.get_blocking_count(0))
        self.assertEqual(arrival + 1 + 6 + 10 + 14, res.get_blocking_term(0))


        res = cpp.phase_fair_rw_bounds(self.rsi_rw, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(2, res.get_blocking_count(15))
        self.assertEqual(1 + 7, res.get_blocking_term(15))

        arrival = 2 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(7, res.get_blocking_count(0))
        # pessimism
        self.assertEqual(arrival + 1 + 14, res.get_blocking_term(0))



    def test_dedicated_irq_pfrw_blocking(self):
        cluster_size = 2
        dedicated_cpu = 0
        res = cpp.phase_fair_rw_bounds(self.rsi, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(6, res.get_blocking_count(15))
        self.assertEqual(1 + 6 + 10 + 7, res.get_blocking_term(15))

        arrival = 1 + 6 + 10 + 14
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(7 + 6, res.get_blocking_count(0))
        self.assertEqual(arrival + 6 + 10 + 14, res.get_blocking_term(0))


        res = cpp.phase_fair_rw_bounds(self.rsi_rw, cluster_size, dedicated_cpu)

        self.assertEqual(0, res.get_arrival_blocking(15))
        self.assertEqual(2, res.get_blocking_count(15))
        self.assertEqual(1 + 7, res.get_blocking_term(15))

        arrival = 1 + 7
        self.assertEqual(arrival, res.get_arrival_blocking(0))
        self.assertEqual(3, res.get_blocking_count(0))
        # pessimism
        self.assertEqual(arrival + 7, res.get_blocking_term(0))


class Test_dpcp_terms(unittest.TestCase):

    def setUp(self):
        self.rsi = cpp.ResourceSharingInfo(4)

        self.rsi.add_task(10, 10, 2, 100)
        self.rsi.add_request(0, 1, 3)

        self.rsi.add_task(25, 25, 3, 200)
        self.rsi.add_request(0, 1, 5)

        self.rsi.add_task(50, 50, 4, 300)
        self.rsi.add_request(0, 1, 7)

        self.rsi.add_task(100, 100, 1, 400)

        self.loc = cpp.ResourceLocality()
        self.loc.assign_resource(0, 1)

    def test_local_blocking(self):
        res = cpp.dpcp_bounds(self.rsi, self.loc)

        self.assertEqual(0, res.get_local_count(0))
        self.assertEqual(0, res.get_local_blocking(0))

        self.assertEqual(0, res.get_local_count(1))
        self.assertEqual(0, res.get_local_blocking(1))

        self.assertEqual(0, res.get_local_count(2))
        self.assertEqual(0, res.get_local_blocking(2))

        self.assertEqual(11 + 5 + 3, res.get_local_count(3))
        self.assertEqual(11 * 3 + 5 * 5 + 3 * 7, res.get_local_blocking(3))

    def test_remote_blocking(self):
        res = cpp.dpcp_bounds(self.rsi, self.loc)

        self.assertEqual(0, res.get_remote_count(3))
        self.assertEqual(0, res.get_remote_blocking(3))

        self.assertEqual(1, res.get_remote_count(0))
        self.assertEqual(7, res.get_remote_blocking(0))

        self.assertEqual(5, res.get_remote_count(1))
        self.assertEqual(4 * 3 + 1 * 7, res.get_remote_blocking(1))

        self.assertEqual(6 + 3, res.get_remote_count(2))
        self.assertEqual(6 * 3 + 3 * 5, res.get_remote_blocking(2))

    def test_priority_ceiling(self):
        self.rsi = cpp.ResourceSharingInfo(4)

        self.rsi.add_task(10, 10, 2, 100)
        self.rsi.add_request(0, 1, 3)

        self.rsi.add_task(25, 25, 3, 200)
        self.rsi.add_request(0, 1, 5)

        self.rsi.add_task(50, 50, 4, 300)
        self.rsi.add_request(1, 1, 7)

        self.rsi.add_task(100, 100, 1, 400)
        self.rsi.add_request(1, 1, 9)

        self.loc = cpp.ResourceLocality()
        self.loc.assign_resource(0, 1)
        self.loc.assign_resource(1, 1)

        res = cpp.dpcp_bounds(self.rsi, self.loc)

        self.assertEqual(1, res.get_remote_count(0))
        self.assertEqual(5, res.get_remote_blocking(0))

        self.assertEqual(4, res.get_remote_count(1))
        self.assertEqual(4 * 3, res.get_remote_blocking(1))

        self.assertEqual(6 + 3 + 1, res.get_remote_count(2))
        self.assertEqual(6 * 3 + 3 * 5 + 1 * 9, res.get_remote_blocking(2))

        self.assertEqual(0, res.get_remote_count(3))
        self.assertEqual(0, res.get_remote_blocking(3))

        self.assertEqual(11 + 5 + 3, res.get_local_count(3))
        self.assertEqual(11 * 3 + 5 * 5 + 3 * 7, res.get_local_blocking(3))


class Test_mpcp_terms(unittest.TestCase):

    def setUp(self):
        self.rsi = cpp.ResourceSharingInfo(4)

        self.rsi.add_task(10, 10, 2, 100)
        self.rsi.add_request(0, 1, 3)

        self.rsi.add_task(25, 25, 3, 200)
        self.rsi.add_request(0, 1, 5)

        self.rsi.add_task(50, 50, 4, 300)
        self.rsi.add_request(0, 1, 7)

        self.rsi.add_task(100, 100, 1, 400)

        self.loc = cpp.ResourceLocality()
        self.loc.assign_resource(0, 1)


    def test_remote_blocking(self):
        res = cpp.mpcp_bounds(self.rsi, False)

        self.assertEqual(0, res.get_remote_count(3))
        self.assertEqual(0, res.get_remote_blocking(3))

        self.assertEqual(0, res.get_blocking_count(3))
        self.assertEqual(0, res.get_blocking_term(3))

        self.assertEqual(7, res.get_remote_blocking(0))
        self.assertEqual(7, res.get_blocking_term(0))

        self.assertEqual(1 * 7 + (2 + 1) * 3, res.get_remote_blocking(1))
        self.assertEqual(1 * 7 + (2 + 1) * 3, res.get_blocking_term(1))

        self.assertEqual((2 + 1) * 3 + (1 + 1) * 5, res.get_remote_blocking(2))
        self.assertEqual((2 + 1) * 3 + (1 + 1) * 5, res.get_blocking_term(2))


class Test_part_fmlp_terms(unittest.TestCase):

    def setUp(self):
        self.rsi = cpp.ResourceSharingInfo(4)

        self.rsi.add_task(10, 10, 2, 100)
        self.rsi.add_request(0, 1, 3)

        self.rsi.add_task(25, 25, 3, 200)
        self.rsi.add_request(0, 1, 5)

        self.rsi.add_task(50, 50, 4, 300)
        self.rsi.add_request(0, 1, 7)

        self.rsi.add_task(100, 100, 1, 400)

        self.loc = cpp.ResourceLocality()
        self.loc.assign_resource(0, 1)


    def test_fmlp_remote(self):
        res = cpp.part_fmlp_bounds(self.rsi, True)

        self.assertEqual(0, res.get_blocking_count(3))
        self.assertEqual(0, res.get_blocking_term(3))

        self.assertEqual(2, res.get_blocking_count(0))
        self.assertEqual(5 + 7, res.get_blocking_term(0))

        self.assertEqual(2, res.get_blocking_count(1))
        self.assertEqual(3 + 7, res.get_blocking_term(1))

        self.assertEqual(2, res.get_blocking_count(2))
        self.assertEqual(3 + 5, res.get_blocking_term(2))


class Test_partition(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(1,  4),
                tasks.SporadicTask(1,  5),
                tasks.SporadicTask(3,  9),
                tasks.SporadicTask(3, 18),
            ])
        r.initialize_resource_model(self.ts)


    def test_singleton(self):
        by_task, by_res = lp.find_connected_components(self.ts)
        self.assertEqual(len(by_res), 0)
        self.assertEqual(len(by_task), len(self.ts))
        for t in self.ts:
            self.assertEqual(len(by_task[t]), 1)
            self.assertTrue(t in by_task[t])

    def test_singleton2(self):
        for i, t in enumerate(self.ts):
            t.resmodel[i].add_request(1)
        by_task, by_res = lp.find_connected_components(self.ts)
        self.assertEqual(len(by_res), 4)
        self.assertEqual(len(by_task), len(self.ts))
        for t in self.ts:
            self.assertEqual(len(by_task[t]), 1)
            self.assertTrue(t in by_task[t])

    def test_merge(self):
        self.ts[0].resmodel[0].add_request(1)
        self.ts[0].resmodel[1].add_request(1)
        self.ts[1].resmodel[1].add_request(1)
        self.ts[1].resmodel[2].add_request(1)
        self.ts[2].resmodel[2].add_request(1)
        self.ts[2].resmodel[3].add_request(1)
        self.ts[3].resmodel[3].add_request(1)

        by_task, by_res = lp.find_connected_components(self.ts)
        self.assertEqual(len(by_res), 4)
        self.assertIs(by_res[0], by_res[1])
        self.assertIs(by_res[1], by_res[2])
        self.assertIs(by_res[2], by_res[3])
        self.assertIs(by_res[3], by_res[0])
        self.assertEqual(len(by_task), len(self.ts))
        for t in self.ts:
            self.assertEqual(len(by_task[t]), 4)
            self.assertIn(t, by_task[t])

    def test_merge2(self):
        self.ts[0].resmodel[0].add_request(1)
        self.ts[0].resmodel[1].add_request(1)
        self.ts[1].resmodel[1].add_request(1)

        self.ts[2].resmodel[2].add_request(1)
        self.ts[2].resmodel[3].add_request(1)
        self.ts[3].resmodel[3].add_request(1)

        by_task, by_res = lp.find_connected_components(self.ts)
        self.assertEqual(len(by_res), 4)
        self.assertIs(by_res[0], by_res[1])
        self.assertNotEqual(by_res[1], by_res[2])
        self.assertIs(by_res[2], by_res[3])
        self.assertNotEqual(by_res[3], by_res[0])
        self.assertEqual(len(by_task), len(self.ts))
        for t in self.ts:
            self.assertEqual(len(by_task[t]), 2)
            self.assertIn(t, by_task[t])

    def test_subsets(self):
        self.ts[0].resmodel[0].add_request(1)
        self.ts[0].resmodel[1].add_request(1)
        self.ts[1].resmodel[1].add_request(1)

        self.ts[2].resmodel[2].add_request(1)
        self.ts[2].resmodel[3].add_request(1)
        self.ts[3].resmodel[3].add_request(1)

        subsets = lp.find_independent_tasksubsets(self.ts)
        self.assertEqual(len(subsets), 2)
        subsets.sort(key=lambda x: x.utilization())
        self.assertIn(self.ts[0], subsets[0])
        self.assertIn(self.ts[1], subsets[0])
        self.assertIn(self.ts[2], subsets[1])
        self.assertIn(self.ts[3], subsets[1])


class Test_linprog(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(25, 200)
        self.t3 = tasks.SporadicTask(33, 33)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        self.ts.assign_ids()
        lb.assign_fp_preemption_levels(self.ts)

        for t in self.ts:
            t.response_time = t.period
            t.partition =  t.id % 2

        self.ts_no_req = self.ts.copy()

        r.initialize_resource_model(self.ts)
        r.initialize_resource_model(self.ts_no_req)

        self.t1.resmodel[0].add_request(1)
        self.t2.resmodel[0].add_request(2)
        self.t3.resmodel[0].add_request(3)

        # only one resource, assigned to the first processor
        self.resource_locality = { 0: 0 }

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dpcp_cpp(self):
        lb.apply_lp_dpcp_bounds(self.ts, self.resource_locality)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dflp_cpp(self):
        lb.apply_lp_dflp_bounds(self.ts, self.resource_locality)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dpcp_cpp_no_req(self):
        lb.apply_lp_dpcp_bounds(self.ts_no_req, {})
        self.assertEqual(self.ts_no_req[0].blocked, 0)
        self.assertEqual(self.ts_no_req[0].suspended, 0)
        self.assertEqual(self.ts_no_req[1].blocked, 0)
        self.assertEqual(self.ts_no_req[1].suspended, 0)
        self.assertEqual(self.ts_no_req[2].blocked, 0)
        self.assertEqual(self.ts_no_req[2].suspended, 0)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dflp_cpp_no_req(self):
        lb.apply_lp_dflp_bounds(self.ts_no_req, {})
        self.assertEqual(self.ts_no_req[0].blocked, 0)
        self.assertEqual(self.ts_no_req[0].suspended, 0)
        self.assertEqual(self.ts_no_req[1].blocked, 0)
        self.assertEqual(self.ts_no_req[1].suspended, 0)
        self.assertEqual(self.ts_no_req[2].blocked, 0)
        self.assertEqual(self.ts_no_req[2].suspended, 0)

class Test_reasonble_priority(unittest.TestCase):

    def setUp(self):
        self.ts = tasks.TaskSystem([
            tasks.SporadicTask(x * 10, x * 100) for x in xrange(1, 10)
            ])

    def test_is_reasonable_priority_assignment(self):
        self.assertTrue(lb.is_reasonable_priority_assignment(1, self.ts))

    def test_is_not_reasonable_priority_assignment(self):
        self.ts[2].deadline = 150
        self.assertFalse(lb.is_reasonable_priority_assignment(1, self.ts))

    def test_skip_top_m_tasks(self):
        self.ts[2].deadline = 150
        self.assertTrue(lb.is_reasonable_priority_assignment(2, self.ts))


import schedcat.sched.fp.rta as rta

class RTABlockingAccounting(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                tasks.SporadicTask(10,  100),
                tasks.SporadicTask(10,  100),
                tasks.SporadicTask(10,  100),
                tasks.SporadicTask(10,  100),
            ])

        r.initialize_resource_model(self.ts)

        # dummy response-time guess
        for t in self.ts:
            t.response_time = 40

        # every task holds the resource for up to 1ms
        for t in self.ts:
            t.resmodel[0].add_request(1)

        # assign tasks to partitions
        self.ts[0].partition = 0
        self.ts[1].partition = 0
        self.ts[2].partition = 1
        self.ts[3].partition = 1

        self.p0 = tasks.TaskSystem((t for t in self.ts if t.partition == 0))
        self.p1 = tasks.TaskSystem((t for t in self.ts if t.partition == 1))

        # assign preemption levels
        lb.assign_fp_preemption_levels(self.ts)


    def test_no_double_accounting_spin(self):
        "test that spin delay and arrival blocking are correctly accounted for"
        lb.apply_task_fair_mutex_bounds(self.ts, 1, pi_aware=True)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival blocking of up to 2ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 1)
        self.assertEqual(self.p1[0].cost, 10 + 1)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 1ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 1 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 1 + 1)

    def test_no_double_accounting_spin_sob(self):
        "test that s-oblivious spin delay and arrival blocking are correctly accounted for"
        lb.apply_task_fair_mutex_bounds(self.ts, 1)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival blocking of up to 2ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 3)
        self.assertEqual(self.p1[0].cost, 10 + 3)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 3ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 3 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 3 + 1)

    def test_no_double_accounting_spin_phase_fair(self):
        "test that spin delay and arrival blocking are correctly accounted for"
        lb.apply_phase_fair_rw_bounds(self.ts, 1, pi_aware=True)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival blocking of up to 2ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 1)
        self.assertEqual(self.p1[0].cost, 10 + 1)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 1ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 1 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 1 + 1)


    def test_no_double_accounting_spin_phase_fair_sob(self):
        "test that s-oblivious spin delay and arrival blocking are correctly accounted for"
        lb.apply_phase_fair_rw_bounds(self.ts, 1)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival blocking of up to 2ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 3)
        self.assertEqual(self.p1[0].cost, 10 + 3)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 3ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 3 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 3 + 1)

    def test_no_double_accounting_spin_msrp(self):
        "test that spin delay and arrival blocking are correctly accounted for"
        lb.apply_msrp_bounds_holistic(self.ts)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival blocking of up to 2ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 1)
        self.assertEqual(self.p1[0].cost, 10 + 1)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 1ms
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 1 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 1 + 1)

    def test_no_double_accounting_sob(self):
        "test that suspension-oblivious analysis is correctly accounted for"
        lb.apply_clustered_omlp_bounds(self.ts, 1)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - arrival sob pi-blocking of up to 2ms
        #   - sob pi-blocking of up to 1ms
        self.assertEqual(self.p0[0].cost, 10 + 2 + 1)
        self.assertEqual(self.p1[0].cost, 10 + 2 + 1)

        self.assertEqual(self.p0[0].response_time, 10 + 2 + 1)
        self.assertEqual(self.p1[0].response_time, 10 + 2 + 1)

        # - for lower-priority task:
        #   - no arrival blocking
        #   - interference of up to 10ms + 3ms (sob inflation)
        #   - spin blocking of up to 1ms
        self.assertEqual(self.p0[1].cost, 10 + 1)
        self.assertEqual(self.p1[1].cost, 10 + 1)

        self.assertEqual(self.p0[1].response_time, 10 + 13 + 1)
        self.assertEqual(self.p1[1].response_time, 10 + 13 + 1)


    def test_no_double_accounting_saw(self):
        "test that suspension-aware analysis is correctly accounted for"
        lb.apply_part_fmlp_bounds(self.ts)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - no WCET inflation
        #   - local pi-blocking of up to 1ms
        #   - remote pi-blocking of up to (1 + 1) ms
        self.assertEqual(self.p0[0].cost, 10)
        self.assertEqual(self.p1[0].cost, 10)

        self.assertEqual(self.p0[0].response_time, 10 + 1 + 2)
        self.assertEqual(self.p1[0].response_time, 10 + 1 + 2)

        # - for lower-priority task:
        #   - no WCET inflation
        #   - no arrival blocking
        #   - interference of up to 10ms (no inflation)
        #   - remote pi-blocking of up to (1 + 1) ms
        self.assertEqual(self.p0[1].cost, 10)
        self.assertEqual(self.p1[1].cost, 10)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 2)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 2)

    @unittest.skipIf(not lb.lp_cpp_available, "no native LP solver available")
    def test_no_double_accounting_saw_lp(self):
        "test that LP-based s-aware analysis is correctly accounted for"
        lb.apply_lp_part_fmlp_bounds(self.ts)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - no WCET inflation
        #   - local pi-blocking of up to 1ms
        #   - remote pi-blocking of up to (1 + 1) ms
        self.assertEqual(self.p0[0].cost, 10)
        self.assertEqual(self.p1[0].cost, 10)

        self.assertEqual(self.p0[0].response_time, 10 + 1 + 2)
        self.assertEqual(self.p1[0].response_time, 10 + 1 + 2)

        # - for lower-priority task:
        #   - no WCET inflation
        #   - no arrival blocking
        #   - interference of up to 10ms (no inflation)
        #   - remote pi-blocking of up to (1 + 1) ms
        self.assertEqual(self.p0[1].cost, 10)
        self.assertEqual(self.p1[1].cost, 10)

        self.assertEqual(self.p0[1].response_time, 10 + 10 + 2)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 2)


    @unittest.skipIf(not lb.lp_cpp_available, "no native LP solver available")
    def test_no_double_accounting_mpcp(self):
        "test that s-aware MPCP analysis is correctly accounted for"
        lb.apply_lp_mpcp_bounds(self.ts)

        self.assertTrue(rta.is_schedulable(1, self.p0))
        self.assertTrue(rta.is_schedulable(1, self.p1))

        # Expected inflated WCET and response times:

        # - for highest-priority task:
        #   - no WCET inflation
        #   - local pi-blocking of up to 1ms
        #   - remote pi-blocking of up to 1 ms // one lower-prio remote CS
        self.assertEqual(self.p0[0].cost, 10)
        self.assertEqual(self.p0[0].response_time, 10 + 1 + 1)

        # on other core, can be blocked by multiple remote tasks due to lower
        # priority => up to 2ms remote, 1ms local
        self.assertEqual(self.p1[0].cost, 10)
        self.assertEqual(self.p1[0].response_time, 10 + 1 + 2)

        # - for lower-priority task on P0:
        #   - no WCET inflation
        #   - no arrival blocking
        #   - interference of up to 10ms (no inflation)
        #   - remote pi-blocking of up to up to 1 ms // one lower-prio remote CS
        self.assertEqual(self.p0[1].cost, 10)
        self.assertEqual(self.p0[1].response_time, 10 + 10 + 1)

        # on other core, repeated blocking is again possible
        self.assertEqual(self.p1[1].cost, 10)
        self.assertEqual(self.p1[1].response_time, 10 + 10 + 2)


class Test_nested_resource_model(unittest.TestCase):
    def setUp(self):
        self.m = r.OutermostCriticalSections()

        res_id = 8
        self.outer1 = self.m.add_outermost(res_id, 1)
        self.outer2 = self.m.add_outermost(res_id, 1)

        res_id = 9
        self.nested1 = self.m.add_nested(self.outer2, res_id, 1)
        self.nested2 = self.m.add_nested(self.outer2, res_id, 1)

        res_id = 10
        self.nested3 = self.m.add_nested(self.nested2, res_id, 1)
        self.nested4 = self.m.add_nested(self.outer2, res_id, 1)

    def test_no_reentrant_locks(self):
        self.assertEqual(len(self.m), 2)
        self.assertEqual(self.m[0].res_id, 8)

        with self.assertRaises(AssertionError):
            self.m.add_nested(self.outer1, 8, 1)

            self.m.add_nested(self.nested, 8, 1)

    def test_iterator(self):

        all = [cs for cs in self.m.all()]
        self.assertEqual([self.outer1, self.outer2, self.nested1, self.nested2,
                          self.nested3, self.nested4], all)

        outer = [cs for cs in self.m.outer()]
        self.assertEqual([self.outer1, self.outer2], outer)

        all_flat = [x for x in self.m.all_flat()]
        expected = [
            ( 8, 1, -1),
            ( 8, 1, -1),
            ( 9, 1,  1),
            ( 9, 1,  1),
            (10, 1,  3),
            (10, 1,  1)
        ]
        self.assertEqual(expected, all_flat)


    def test_well_ordered_nesting(self):
        self.assertTrue(self.m.all_nesting_well_ordered())

        self.m.add_nested(self.outer1, 1, 1)

        self.assertFalse(self.m.all_nesting_well_ordered())

    def test_total_length(self):
        self.assertEqual(self.m[0].total_length, 1)
        self.assertEqual(self.m[1].total_length, 5)


class Test_group_locks(unittest.TestCase):

    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(25, 200)
        self.t3 = tasks.SporadicTask(33, 33)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        self.t1.critical_sections.add_outermost(1, 1)
        self.t2.critical_sections.add_outermost(2, 1)
        self.t3.critical_sections.add_outermost(3, 1)

    def test_trivial_groups(self):
        groups = r.identify_group_locks(self.ts)

        self.assertEqual(groups[1], set([1]))
        self.assertEqual(groups[2], set([2]))
        self.assertEqual(groups[3], set([3]))

    def test_nested_groups(self):
        self.t3.critical_sections.add_nested(self.t3.critical_sections[0], 2, 1)

        groups = r.identify_group_locks(self.ts)

        self.assertEqual(groups[1], set([1]))
        self.assertEqual(groups[2], set([2, 3]))
        self.assertEqual(groups[3], set([2, 3]))
        self.assertIs(groups[3], groups[2])

    def test_collapsed_groups(self):
        self.t3.critical_sections.add_nested(self.t3.critical_sections[0], 2, 1)
        self.t1.critical_sections.add_nested(self.t1.critical_sections[0], 2, 1)
        groups = r.identify_group_locks(self.ts)

        self.assertEqual(groups[1], set([1, 2, 3]))
        self.assertEqual(groups[2], set([1, 2, 3]))
        self.assertEqual(groups[3], set([1, 2, 3]))
        self.assertIs(groups[3], groups[2])
        self.assertIs(groups[1], groups[2])

    def test_trivial_group_lock_model(self):
        self.t1.critical_sections.add_outermost(1, 1)
        self.t2.critical_sections.add_outermost(2, 1)
        self.t3.critical_sections.add_outermost(3, 1)

        self.t1.critical_sections.add_outermost(2, 1)
        self.t2.critical_sections.add_outermost(3, 1)
        self.t3.critical_sections.add_outermost(1, 1)

        self.t1.critical_sections.add_outermost(2, 3)
        self.t2.critical_sections.add_outermost(3, 3)
        self.t3.critical_sections.add_outermost(1, 3)

        groups = r.identify_group_locks(self.ts)
        model = self.t1.critical_sections.get_group_lock_model(groups)
        self.assertEqual(len(model), 2)
        self.assertEqual(model[1].max_requests, 2)
        self.assertEqual(model[1].max_length, 1)
        self.assertEqual(model[2].max_requests, 2)
        self.assertEqual(model[2].max_length, 3)

        model = self.t2.critical_sections.get_group_lock_model(groups)
        self.assertEqual(len(model), 2)
        self.assertEqual(model[2].max_requests, 2)
        self.assertEqual(model[2].max_length, 1)
        self.assertEqual(model[3].max_requests, 2)
        self.assertEqual(model[3].max_length, 3)

    def test_group_lock_model(self):
        self.t1.critical_sections.add_outermost(1, 1)
        self.t2.critical_sections.add_outermost(2, 1)
        self.t3.critical_sections.add_outermost(3, 1)

        self.t1.critical_sections.add_outermost(2, 1)
        self.t2.critical_sections.add_outermost(3, 1)
        self.t3.critical_sections.add_outermost(1, 1)

        self.t1.critical_sections.add_outermost(2, 3)
        self.t2.critical_sections.add_outermost(3, 3)
        self.t3.critical_sections.add_outermost(1, 3)

        self.t3.critical_sections.add_nested(self.t3.critical_sections[0], 2, 1)
        self.t3.critical_sections.add_nested(self.t3.critical_sections[-1], 3, 1)
        self.t1.critical_sections.add_nested(self.t1.critical_sections[0], 2, 7)

        groups = r.identify_group_locks(self.ts)

        model = self.t1.critical_sections.get_group_lock_model(groups)
        self.assertEqual(len(model), 1)
        self.assertEqual(model[1].max_requests, 4)
        self.assertEqual(model[1].max_length, 8)

        model = self.t2.critical_sections.get_group_lock_model(groups)
        self.assertEqual(len(model), 1)
        self.assertEqual(model[1].max_requests, 4)
        self.assertEqual(model[1].max_length, 3)

        model = self.t3.critical_sections.get_group_lock_model(groups)
        self.assertEqual(len(model), 1)
        self.assertEqual(model[1].max_requests, 4)
        self.assertEqual(model[1].max_length, 4)


class Test_cpp_non_nested_analysis(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        self.t1.critical_sections.add_outermost(1, 1)

        self.t2.critical_sections.add_outermost(1, 5)

        self.t3.critical_sections.add_outermost(1, 7)

        for i, t in enumerate(self.ts):
            t.response_time = 20

        self.t1.partition = 0
        self.t2.partition = 1
        self.t3.partition = 1

        lb.assign_fp_preemption_levels(self.ts)

    def test_classic_msrp(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_msrp_bounds(self.ts, 2)

        self.assertEqual(self.t1.blocked, 0)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 0)

        self.assertEqual(self.t1.remote_blocking, 7)
        self.assertEqual(self.t2.remote_blocking, 1)
        self.assertEqual(self.t3.remote_blocking, 1)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 1)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 1)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks2(self):
        self.t2.critical_sections.add_outermost(1, 5)
        self.t3.critical_sections.add_outermost(1, 7)

        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 1)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo2(self):
        self.t2.critical_sections.add_outermost(1, 5)
        self.t3.critical_sections.add_outermost(1, 7)
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 1)


class Test_cpp_non_nested_analysis2(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        self.t1.critical_sections.add_outermost(1, 1)

        self.t2.critical_sections.add_outermost(1, 5)
        self.t2.critical_sections.add_outermost(1, 5)

        self.t3.critical_sections.add_outermost(1, 7)
        self.t3.critical_sections.add_outermost(1, 7)

        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 7 )
        self.assertEqual(self.t2.blocked, 1 + 1 + 7 + 7)
        self.assertEqual(self.t3.blocked, 1 + 1 + 5 + 5)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 7)
        self.assertEqual(self.t2.blocked, 1 + 1 + 7 + 7)
        self.assertEqual(self.t3.blocked, 1 + 1 + 5 + 5)

class Test_cpp_nested_analysis_trivial1(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        cs = self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_nested(cs, 2, 1)

        cs = self.t2.critical_sections.add_outermost(1, 5)
        self.t2.critical_sections.add_nested(cs, 2, 5)

        cs = self.t3.critical_sections.add_outermost(1, 7)
        self.t3.critical_sections.add_nested(cs, 2, 7)

        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 5 + 7 + 7)
        self.assertEqual(self.t2.blocked, 1 + 1 + 7 + 7)
        self.assertEqual(self.t3.blocked, 1 + 1 + 5 + 5)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 5 + 7 + 7)
        self.assertEqual(self.t2.blocked, 1 + 1 + 7 + 7)
        self.assertEqual(self.t3.blocked, 1 + 1 + 5 + 5)


class Test_cpp_nested_analysis_trivial2(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        # T1
        cs = self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_nested(cs, 2, 1)

        # T2
        cs = self.t2.critical_sections.add_outermost(2, 5)
        self.t2.critical_sections.add_nested(cs, 3, 5)

        # T3
        self.t3.critical_sections.add_outermost(1, 20)
        cs = self.t3.critical_sections.add_outermost(1, 7)
        self.t3.critical_sections.add_nested(cs, 3, 7)



        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 5 + 20)
        self.assertEqual(self.t2.blocked, 1 + 1 + 20)
        self.assertEqual(self.t3.blocked, 2 * (1 + 1 + 5 + 5))


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 5 + 5 + 20)
        self.assertEqual(self.t2.blocked, 1 + 7)
        self.assertEqual(self.t3.blocked, (1 + 1) + 10 + (1 + 1) + 10)


class Test_cpp_nested_analysis_double_indirect(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.t4 = tasks.SporadicTask(10, 100)
        self.t5 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3, self.t4, self.t5])

        r.initialize_nested_resource_model(self.ts)

        # T1
        cs = self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_nested(cs, 2, 1)

        # T2
        cs = self.t2.critical_sections.add_outermost(2, 2)
        self.t2.critical_sections.add_nested(cs, 3, 2)

        # T3
        cs = self.t3.critical_sections.add_outermost(3, 3)
        self.t3.critical_sections.add_nested(cs, 4, 3)

        # T4
        self.t4.critical_sections.add_outermost(1, 10)
        cs = self.t4.critical_sections.add_outermost(1, 4)
        self.t4.critical_sections.add_nested(cs, 4, 4)


        for i, t in enumerate(self.ts):
            t.response_time = 40
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    def extra_requests(self):
        # T5
        cs = self.t5.critical_sections.add_outermost(0, 5)
        self.t5.critical_sections.add_nested(cs, 4, 5)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 4 + 6 + 10)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        # Cannot block on T4's nested request via nested chain through T2, T3
        # because of implicit synchronization through T1.
        self.assertEqual(self.t1.blocked, 4 + 6 + 10)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks2(self):
        self.extra_requests()
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 4 + 6 + 10 + 10)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo2(self):
        self.extra_requests()
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        # Cannot block on T4's nested request via nested chain through T2, T3
        # because of implicit synchronization through T1.
        # It can block on T5's nested request.
        self.assertEqual(self.t1.blocked, 4 + 6 + 10 + 5)


class Test_cpp_nested_analysis_trivial3(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2])

        r.initialize_nested_resource_model(self.ts)

        cs = self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_nested(cs, 3, 1)

        cs = self.t2.critical_sections.add_outermost(1, 3)
        cs = self.t2.critical_sections.add_nested(cs, 2, 3)
        self.t2.critical_sections.add_nested(cs, 3, 3)

        cs = self.t2.critical_sections.add_outermost(1, 5)
        cs = self.t2.critical_sections.add_nested(cs, 3, 5)

        for i, t in enumerate(self.ts):
            t.response_time = 40
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    def extra_requests(self):
        self.t1.critical_sections.add_outermost(3, 1)
        self.t2.critical_sections.add_outermost(3, 7)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks1(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 1 * 10)
        self.assertEqual(self.t2.blocked, 1 * 2)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo1(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        # (1,1)-(3,1) of T1 directly blocks on (1,5)-(3,5) of T2.
        expected = 5 + 5
        # ...and on nothing else due to implicit serialization via resource 1
        self.assertEqual(self.t1.blocked, expected)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks2(self):
        self.extra_requests()
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 2 * 10)
        self.assertEqual(self.t2.blocked, 2 * 2)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo2(self):
        self.extra_requests()
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        # First, (1,1)-(3,1) of T1 directly blocks on (1,3)-(2,3)-(3,3) of T2
        expected = 3 + 3 + 3
        # then on (3, 7) of T2.
        expected += 7
        # Second, (3,1) of T1 directly blocks on the nested request of (1,5)-(3,5) of T2.
        expected += 5
        self.assertEqual(self.t1.blocked, expected)


class Test_cpp_nested_analysis_implicitly_serialized_across_cores(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 100)
        self.t3 = tasks.SporadicTask(10, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        cs = self.t1.critical_sections.add_outermost(2, 1)

        cs = self.t2.critical_sections.add_outermost(1, 2)
        self.t2.critical_sections.add_nested(cs, 2, 2)

        cs = self.t3.critical_sections.add_outermost(1, 3)
        self.t3.critical_sections.add_nested(cs, 2, 3)

        for i, t in enumerate(self.ts):
            t.response_time = 40
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks1(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 1 * 4 + 1 * 6)
        self.assertEqual(self.t2.blocked, 1 * 1 + 1 * 6)
        self.assertEqual(self.t3.blocked, 1 * 1 + 1 * 4)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    # The actual expected blocking would be 3. However, due to pessimism
    # in the analysis, it is actually reported as 5. This test case
    # serves as a reminder of this scenario, so it's marked as an
    # expected failure.
    @unittest.expectedFailure
    def test_fifo1(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        # (2,1) of T1 directly blocks on (1,3)-(2,3) of T3.
        expected = 3
        # ...and on nothing else due to implicit serialization via resource 1
        self.assertEqual(self.t1.blocked, expected)


class Test_cpp_nested_analysis_max_number_nested(unittest.TestCase):
    def setUp(self):
        self.n = 8
        self.ts = tasks.TaskSystem(
            [tasks.SporadicTask(10, 100) for i in xrange(self.n)])
        r.initialize_nested_resource_model(self.ts)

        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i
            t.critical_sections.add_outermost(1, 1)

        self.ts[1].critical_sections.add_nested(
            self.ts[1].critical_sections[0], 2, 1)
        self.ts[2].critical_sections.add_nested(
            self.ts[2].critical_sections[0], 2, 1)

        for i, t in enumerate(self.ts[1:]):
            t.critical_sections.add_outermost(2, 2)
            t.critical_sections.add_outermost(2, 2)
            t.critical_sections.add_outermost(1, 1)
            t.critical_sections.add_outermost(1, 1)

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.ts[0].blocked, (self.n - 1) * 2)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.ts[0].blocked, (self.n - 1) * 1 + 1 + 1 + (self.n - 2) * 2 * 2)


class Test_cpp_np_fifo_no_loop_back(unittest.TestCase):
    def setUp(self):
        self.n = 8
        self.ts = tasks.TaskSystem(
            [tasks.SporadicTask(10, 100) for i in xrange(self.n)])
        r.initialize_nested_resource_model(self.ts)

        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i
            t.critical_sections.add_outermost(1, i + 1)

        lb.assign_fp_preemption_levels(self.ts)

    def test_classic_msrp(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_msrp_bounds(self.ts, self.n)

        for i, t in enumerate(self.ts):
            self.assertEqual(t.remote_blocking,\
                             sum([j + 1 for j in xrange(self.n) if j != i]))


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        for i, t in enumerate(self.ts):
            self.assertEqual(t.blocked,\
                             sum([j + 1 for j in xrange(self.n) if j != i]))

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        for i, t in enumerate(self.ts):
            self.assertEqual(t.blocked,\
                             sum([j + 1 for j in xrange(self.n) if j != i]))


class Test_cpp_fifo_spin_locks_pcp(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(10, 120)
        self.t3 = tasks.SporadicTask(10, 130)
        self.t4 = tasks.SporadicTask(10, 140)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3, self.t4])

        r.initialize_nested_resource_model(self.ts)

        self.t1.critical_sections.add_outermost(1, 1)

        self.t2.critical_sections.add_outermost(1, 5)

        self.t3.critical_sections.add_outermost(1, 7)
        self.t3.critical_sections.add_outermost(2, 10)

        self.t4.critical_sections.add_outermost(2, 20)

        for i, t in enumerate(self.ts):
            t.response_time = 45

        self.t1.partition = 0
        self.t2.partition = 1
        self.t3.partition = 1
        self.t4.partition = 1

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_classic_msrp(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_msrp_bounds(self.ts, 2)

        self.assertEqual(self.t1.blocked, 0)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 20)
        self.assertEqual(self.t4.blocked, 0)

        self.assertEqual(self.t1.remote_blocking, 7)
        self.assertEqual(self.t2.remote_blocking, 1)
        self.assertEqual(self.t3.remote_blocking, 1)
        self.assertEqual(self.t4.remote_blocking, 0)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_group_locks(self):
        r.convert_to_group_locks(self.ts)
        res = lb.apply_pfp_lp_msrp_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 21)
        self.assertEqual(self.t4.blocked, 1) # account for higher-prio spin delay

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_fifo(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 7)
        self.assertEqual(self.t2.blocked, 8)
        self.assertEqual(self.t3.blocked, 1 + 20)
        self.assertEqual(self.t4.blocked, 1) # account for higher-prio spin delay

class Test_cpp_nested_analysis(unittest.TestCase):
    def setUp(self):
        self.t1 = tasks.SporadicTask(10, 100)
        self.t2 = tasks.SporadicTask(25, 100)
        self.t3 = tasks.SporadicTask(33, 100)
        self.ts = tasks.TaskSystem([self.t1, self.t2, self.t3])

        r.initialize_nested_resource_model(self.ts)

        self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_nested(self.t1.critical_sections[0], 2, 7)
        self.t1.critical_sections.add_outermost(1, 1)
        self.t1.critical_sections.add_outermost(2, 1)
        self.t1.critical_sections.add_outermost(2, 3)
#        self.t1.critical_sections.add_outermost(4, 1)

        self.t2.critical_sections.add_outermost(2, 1)
        self.t2.critical_sections.add_outermost(2, 1)
        self.t2.critical_sections.add_outermost(3, 1)
        self.t2.critical_sections.add_outermost(3, 3)
#        self.t2.critical_sections.add_outermost(4, 2)

        self.t3.critical_sections.add_nested(self.t2.critical_sections[0], 3, 1)
        self.t3.critical_sections.add_outermost(3, 1)
        self.t3.critical_sections.add_outermost(3, 1)
        self.t3.critical_sections.add_outermost(1, 1)
        self.t3.critical_sections.add_outermost(1, 3)
        self.t3.critical_sections.add_nested(self.t3.critical_sections[-1], 3, 1)
#        self.t3.critical_sections.add_outermost(4, 3)


        for i, t in enumerate(self.ts):
            t.response_time = t.deadline
            t.partition = i

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_cpp_model_generation(self):
        # just check that it doesn't crash
        model = lb.get_cpp_nested_cs_model(self.ts)
        # lb.lp_cpp.dump(model)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_apply_nested_fifo_bounds(self):
        res = lb.apply_pfp_nested_fifo_spinlock_bounds(self.ts)

        self.assertEqual(self.t1.blocked, 21)
        self.assertEqual(self.t2.blocked, 17)
        self.assertEqual(self.t3.blocked, 27)
