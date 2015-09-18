import unittest

import schedcat.model.canbus as c
import schedcat.sched.canbus.broster as b

class CANMessage1(unittest.TestCase):
    def setUp(self):
        self.m1 = c.CANMessage(8, 10)
        self.m2 = c.CANMessage(7, 20, 15)
        self.m3 = c.CANMessage(6, 20, 15, 2, 1)
        self.m4 = c.CANMessage(5, 50, id=4, tid=2)
        self.busrate = 250 # bits / ms
        self.tau = 1.0 / self.busrate

    def test_ids(self):
        self.assertEqual(self.m1.id, None)
        self.assertEqual(self.m1.tid, None)
        self.assertEqual(self.m2.id, None)
        self.assertEqual(self.m2.tid, None)
        self.assertEqual(self.m3.id, 2)
        self.assertEqual(self.m3.tid, 1)
        self.assertEqual(self.m4.id, 4)
        self.assertEqual(self.m4.tid, 2)

    def test_max_framesize(self):
        self.assertEqual(self.m1.max_framesize, 132)
        self.assertEqual(self.m2.max_framesize, 122)
        self.assertEqual(self.m3.max_framesize, 112)
        self.assertEqual(self.m4.max_framesize, 102)
        self.assertEqual(self.m1.max_framesize, 52 + 10 * self.m1.cost)
        self.assertEqual(self.m2.max_framesize, 52 + 10 * self.m2.cost)
        self.assertEqual(self.m3.max_framesize, 52 + 10 * self.m3.cost)
        self.assertEqual(self.m4.max_framesize, 52 + 10 * self.m4.cost)

    def test_utilization(self):
        self.assertEqual(round(self.m1.utilization(self.tau), 4), 0.0528)
        self.assertEqual(round(self.m2.utilization(self.tau), 4), 0.0244)
        self.assertEqual(round(self.m3.utilization(self.tau), 4), 0.0224)
        self.assertEqual(round(self.m4.utilization(self.tau), 5), 0.00816)

class CANMessage2(unittest.TestCase):
    def setUp(self):
        """The taskset is taken from Broster et al.'s 2002 paper, "Probabilistic
        Analysis of CAN with Faults". Unlike the paper, where 12 is the highest 
        priority, and 1 the lowest, we assume 1 as the highest priority and
        12 as the highest priority for this taskset.
        """
        ms = []
        ms.append(c.CANMessage(8, 10, id=1))
        ms.append(c.CANMessage(3, 14, id=2))
        ms.append(c.CANMessage(3, 20, id=3))
        ms.append(c.CANMessage(2, 15, id=4))
        ms.append(c.CANMessage(5, 20, id=5))
        ms.append(c.CANMessage(5, 40, id=6))
        ms.append(c.CANMessage(4, 15, id=7))
        ms.append(c.CANMessage(5, 50, id=8))
        ms.append(c.CANMessage(4, 20, id=9))
        ms.append(c.CANMessage(7, 100, id=10))
        ms.append(c.CANMessage(5, 50, id=11))
        ms.append(c.CANMessage(1, 100, id=12))
        self.ms = c.CANMessageSet(ms)
        self.ms.busrate = 250 # bits / ms
        self.ms.tau = 1.0 / self.ms.busrate
        self.ms.inter_frame_space = 3 * self.ms.tau
        self.ms.max_error_frame_size = 29 * self.ms.tau
        self.ms.mfr = 0.03 # faults / ms

    def test_wctts_wo_faults(self):
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 0), 3), 1.028)
        self.assertEqual(round(self.ms.get_wctt(self.ms[1], 0), 3), 1.368)
        self.assertEqual(round(self.ms.get_wctt(self.ms[2], 0), 3), 1.708)
        self.assertEqual(round(self.ms.get_wctt(self.ms[3], 0), 3), 2.008)
        self.assertEqual(round(self.ms.get_wctt(self.ms[4], 0), 3), 2.428)
        self.assertEqual(round(self.ms.get_wctt(self.ms[5], 0), 3), 2.848)
        self.assertEqual(round(self.ms.get_wctt(self.ms[6], 0), 3), 3.228)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 0), 3), 3.648)
        self.assertEqual(round(self.ms.get_wctt(self.ms[8], 0), 3), 4.028)
        self.assertEqual(round(self.ms.get_wctt(self.ms[9], 0), 3), 4.448)
        self.assertEqual(round(self.ms.get_wctt(self.ms[10], 0), 3), 4.708)
        self.assertEqual(round(self.ms.get_wctt(self.ms[11], 0), 3), 4.720)
    
    def test_wctts_w_faults(self):
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 0), 3), 1.028)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 1), 3), 1.672)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 2), 3), 2.316)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 3), 3), 2.960)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 4), 3), 3.604)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 5), 3), 4.248)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 6), 3), 4.892)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 7), 3), 5.536)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 8), 3), 6.180)
        self.assertEqual(round(self.ms.get_wctt(self.ms[0], 9), 3), 6.824)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 0), 3), 3.648)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 1), 3), 4.292)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 2), 3), 4.936)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 3), 3), 5.580)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 4), 3), 6.224)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 5), 3), 6.868)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 6), 3), 7.512)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 7), 3), 8.156)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 8), 3), 8.800)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 9), 3), 9.444)
        self.assertEqual(round(self.ms.get_wctt(self.ms[7], 10), 3), 10.088)
    
    def test_wctts_fast_w_faults(self):
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 0), 3), 1.028)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 1), 3), 1.672)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 2), 3), 2.316)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 3), 3), 2.960)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 4), 3), 3.604)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 5), 3), 4.248)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 6), 3), 4.892)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 7), 3), 5.536)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 8), 3), 6.180)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[0], 9), 3), 6.824)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 0), 3), 3.648)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 1), 3), 4.292)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 2), 3), 4.936)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 3), 3), 5.580)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 4), 3), 6.224)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 5), 3), 6.868)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 6), 3), 7.512)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 7), 3), 8.156)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 8), 3), 8.800)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 9), 3), 9.444)
        self.assertEqual(round(self.ms.get_wctt_fast(self.ms[7], 10), 3), 10.088)

    def test_broster_get_prob_schedulable(self):
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 0), 6), 0.969631)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 1), 7), 0.0293312)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 2), 9), 0.000999469)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 3), 10), 3.70872e-05)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 4), 11), 1.45769e-06)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 5), 13), 5.96774e-08)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[0], 6), 14), 2.51816e-09)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 0), 6), 0.896336)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 1), 6), 0.096218)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 2), 8), 0.00698767)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 3), 9), 0.000432349)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 4), 10), 2.46289e-05)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 5), 11), 1.33758e-06)
        self.assertEqual(round(b.get_prob_schedulable(self.ms, self.ms[7], 6), 12), 7.0527e-08)

class CANMessage3(unittest.TestCase):
    def setUp(self):
        ms = [  c.CANMessage(8, 10, tid=1, id=1), \
                c.CANMessage(7, 20, tid=2, id=2), \
                c.CANMessage(6, 20, tid=3, id=3), \
                c.CANMessage(5, 50, tid=4, id=4)    ]
        self.ms = c.CANMessageSet(ms)
        self.ms.busrate = 250 # bits / ms
        self.ms.tau = 1.0 / self.ms.busrate
        self.ms.inter_frame_space = 3 * self.ms.tau
        self.ms.max_error_frame_size = 29 * self.ms.tau
        self.ms.mfr = 0.03 # faults / ms

    def test_create_replicas(self):
        len_old = len(self.ms)
        for i in range(0, len(self.ms)):
            self.ms.add_replicas(self.ms[i], 1)
        self.assertEqual(len(self.ms), len_old * 2) 
        self.ms.add_replicas(self.ms[0], 1)
        self.assertEqual(len(self.ms), (len_old * 2) + 1)
        self.ms.add_replicas(self.ms[0], 4)
        self.assertEqual(len(self.ms), (len_old * 2) + 5)

    def test_replication_factor(self):
        self.assertEqual(self.ms.get_replication_factor(self.ms[0]), 1)
        self.assertEqual(self.ms.get_replication_factor(self.ms[1]), 1)
        self.ms.add_replicas(self.ms[1], 1)
        self.assertEqual(self.ms.get_replication_factor(self.ms[0]), 1)
        self.assertEqual(self.ms.get_replication_factor(self.ms[1]), 2)
        self.ms.add_replicas(self.ms[1], 2)
        self.assertEqual(self.ms.get_replication_factor(self.ms[0]), 1)
        self.assertEqual(self.ms.get_replication_factor(self.ms[1]), 4)
