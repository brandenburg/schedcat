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

    def sob_non_zero_blocking(self):
        for t, t_ in zip(self.ts, self.ts_):
            self.assertGreater(t.blocked, 0)
            self.assertEqual(t.suspended, 0)
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
        self.sob_non_zero_blocking()

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
        lb.apply_lp_dpcp_bounds(self.ts, self.resource_locality, use_py=False)

    @unittest.skipIf(not schedcat.util.linprog.cplex_available, "no LP solver available")
    def test_dpcp_py(self):
        lb.apply_lp_dpcp_bounds(self.ts, self.resource_locality, use_py=True)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dflp_cpp(self):
        lb.apply_lp_dflp_bounds(self.ts, self.resource_locality, use_py=False)

    @unittest.skipIf(not schedcat.util.linprog.cplex_available, "no LP solver available")
    def test_dflp_py(self):
        lb.apply_lp_dflp_bounds(self.ts, self.resource_locality, use_py=True)


    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dpcp_cpp_no_req(self):
        lb.apply_lp_dpcp_bounds(self.ts_no_req, {}, use_py=False)
        self.assertEqual(self.ts_no_req[0].blocked, 0)
        self.assertEqual(self.ts_no_req[0].suspended, 0)
        self.assertEqual(self.ts_no_req[1].blocked, 0)
        self.assertEqual(self.ts_no_req[1].suspended, 0)
        self.assertEqual(self.ts_no_req[2].blocked, 0)
        self.assertEqual(self.ts_no_req[2].suspended, 0)

    @unittest.skipIf(not schedcat.util.linprog.cplex_available, "no LP solver available")
    def test_dpcp_py_no_req(self):
        lb.apply_lp_dpcp_bounds(self.ts_no_req, {}, use_py=True)
        self.assertEqual(self.ts_no_req[0].blocked, 0)
        self.assertEqual(self.ts_no_req[0].suspended, 0)
        self.assertEqual(self.ts_no_req[1].blocked, 0)
        self.assertEqual(self.ts_no_req[1].suspended, 0)
        self.assertEqual(self.ts_no_req[2].blocked, 0)
        self.assertEqual(self.ts_no_req[2].suspended, 0)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_dflp_cpp_no_req(self):
        lb.apply_lp_dflp_bounds(self.ts_no_req, {}, use_py=False)
        self.assertEqual(self.ts_no_req[0].blocked, 0)
        self.assertEqual(self.ts_no_req[0].suspended, 0)
        self.assertEqual(self.ts_no_req[1].blocked, 0)
        self.assertEqual(self.ts_no_req[1].suspended, 0)
        self.assertEqual(self.ts_no_req[2].blocked, 0)
        self.assertEqual(self.ts_no_req[2].suspended, 0)

    @unittest.skipIf(not schedcat.util.linprog.cplex_available, "no LP solver available")
    def test_dflp_py_no_req(self):
        lb.apply_lp_dflp_bounds(self.ts_no_req, {}, use_py=True)
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
