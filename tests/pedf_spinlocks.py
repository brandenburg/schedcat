from __future__ import division

import unittest
import random

import schedcat.locking.bounds as lb
import schedcat.locking.native as cpp
import schedcat.model.tasks as tasks
import schedcat.model.resources as r

import schedcat.util.linprog

class PEDF_Spinlocks(unittest.TestCase):
    def setUp(self):
        self.trivial_ts = tasks.TaskSystem([
                tasks.SporadicTask(10,   33,  24),
                tasks.SporadicTask(10,   95,  81),
                tasks.SporadicTask(12,  63, 63),
                #
                tasks.SporadicTask(10,  30, 30),
                tasks.SporadicTask(20,  150, 130),
            ])
        self.trivial_num_cpus = 2


        r.initialize_resource_model(self.trivial_ts)
        lb.assign_edf_preemption_levels(self.trivial_ts)

        #for i, t in enumerate(self.trivial_ts):
        #    t.partition = 0
        #    t.response_time = 4*t.cost

        # CPU0
        self.trivial_ts[0].partition = 0;
        self.trivial_ts[1].partition = 0;
        self.trivial_ts[2].partition = 0;

        # CPU1
        self.trivial_ts[3].partition = 1;
        self.trivial_ts[4].partition = 1;

        # ---------[ Reuqests ]-----------------------

        # L_{0,0}=3, N_{0,0}=1
        self.trivial_ts[0].resmodel[0].add_request(3)

        # L_{1,0}=3, N_{1,0}=1
        self.trivial_ts[1].resmodel[0].add_request(1)
        #self.trivial_ts[1].resmodel[0].add_request(1)

        # L_{1,1}=5, N_{1,1}=1
        self.trivial_ts[1].resmodel[1].add_request(5)

        # L_{2,0}=5, N_{2,0}=1
        self.trivial_ts[2].resmodel[0].add_request(1)
        self.trivial_ts[2].resmodel[0].add_request(1)
        self.trivial_ts[2].resmodel[0].add_request(1)


        # L_{3,0}=1, N_{3,0}=2
        self.trivial_ts[3].resmodel[0].add_request(1)
        self.trivial_ts[3].resmodel[0].add_request(1)

        # L_{4,0}=1, N_{4,0}=1
        self.trivial_ts[4].resmodel[0].add_request(4)

        # L_{4,1}=1, N_{4,1}=2
        self.trivial_ts[4].resmodel[1].add_request(1)
        self.trivial_ts[4].resmodel[1].add_request(1)

        # L_{4,2}=2, N_{4,2}=1
        self.trivial_ts[4].resmodel[2].add_request(2)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_MSRP(self):
        if lb.pedf_msrp_is_schedulable(self.trivial_ts):
            print "[MSRP] SCHEDULABLE";
        else:
            print "[MSRP] NOT SCHEDULABLE";

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_FIFO_preempt(self):
        if lb.pedf_fifo_preempt_is_schedulable(self.trivial_ts):
            print "[FIFO Preemptive] SCHEDULABLE";
        else:
            print "[FIFO Preemptive] NOT SCHEDULABLE";

    def test_MSRP_classic(self):
        if lb.pedf_msrp_classic_is_schedulable(self.trivial_ts, self.trivial_num_cpus):
            print "[MSRP Classic] SCHEDULABLE";
        else:
            print "[MSRP Classic] NOT SCHEDULABLE";

        #self.assertEqual(self.trivial_ts[0].response_time, self.trivial_ts[0].cost + 75)
