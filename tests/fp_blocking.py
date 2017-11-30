from __future__ import division

import unittest
import random
import copy
import StringIO

import schedcat.model.tasks as tasks
import schedcat.model.resources as r
import schedcat.model.serialize as ser

import schedcat.sched.fp.rta as rta
import schedcat.sched.fp as fp

from collections import defaultdict

import schedcat.locking.bounds as lb
import schedcat.locking.native as cpp
import schedcat.locking.partition as lp

import schedcat.util.linprog

import schedcat.generator.generator_emstada as emstada

from schedcat.mapping.binpack import worst_fit_decreasing


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


TASKSET = """
<taskset>
	<task id="1" partition="8" period="18870" wcet="1538">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="0" max_writes="0" res_id="0" />
		</resources>
	</task>
	<task id="2" partition="4" period="19800" wcet="4057">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="100" max_writes="1" res_id="0" />
		</resources>
	</task>
	<task id="3" partition="3" period="21900" wcet="5711">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="0" max_writes="0" res_id="0" />
		</resources>
	</task>
	<task id="4" partition="7" period="22270" wcet="9085">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="0" max_writes="0" res_id="0" />
		</resources>
	</task>
	<task id="5" partition="6" period="29430" wcet="105">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="100" max_writes="1" res_id="0" />
		</resources>
	</task>
	<task id="6" partition="1" period="31140" wcet="7775">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="100" max_writes="1" res_id="0" />
		</resources>
	</task>
	<task id="7" partition="1" period="32520" wcet="3933">
		<resources />
	</task>
	<task id="8" partition="0" period="33920" wcet="7297">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="0" max_writes="0" res_id="0" />
		</resources>
	</task>
	<task id="9" partition="6" period="36940" wcet="3804">
		<resources />
	</task>
	<task id="10" partition="5" period="41490" wcet="1963">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="0" max_writes="0" res_id="0" />
		</resources>
	</task>
	<task id="11" partition="9" period="45280" wcet="6320">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="100" max_writes="1" res_id="0" />
		</resources>
	</task>
	<task id="12" partition="9" period="47090" wcet="21474">
		<resources />
	</task>
	<task id="13" partition="3" period="48600" wcet="22421">
		<resources />
	</task>
	<task id="14" partition="2" period="48800" wcet="1432">
		<resources>
			<requirement max_read_length="100" max_reads="1" max_write_length="100" max_writes="1" res_id="0" />
		</resources>
	</task>
	<task id="15" partition="3" period="48960" wcet="1367">
		<resources />
	</task>
	<task id="16" partition="5" period="49050" wcet="6393">
		<resources />
	</task>
	<task id="17" partition="0" period="52930" wcet="15045">
		<resources />
	</task>
	<task id="18" partition="2" period="55990" wcet="21411">
		<resources />
	</task>
	<task id="19" partition="9" period="61520" wcet="9501">
		<resources />
	</task>
	<task id="20" partition="1" period="65040" wcet="24677">
		<resources />
	</task>
	<task id="21" partition="2" period="68950" wcet="23324">
		<resources />
	</task>
	<task id="22" partition="7" period="73340" wcet="4522">
		<resources />
	</task>
	<task id="23" partition="5" period="77760" wcet="44509">
		<resources />
	</task>
	<task id="24" partition="7" period="80440" wcet="22556">
		<resources />
	</task>
	<task id="25" partition="4" period="81930" wcet="4770">
		<resources />
	</task>
	<task id="26" partition="4" period="86190" wcet="41966">
		<resources />
	</task>
	<task id="27" partition="8" period="89250" wcet="46161">
		<resources />
	</task>
	<task id="28" partition="0" period="91590" wcet="22959">
		<resources />
	</task>
	<task id="29" partition="6" period="93240" wcet="59997">
		<resources />
	</task>
	<task id="30" partition="8" period="94300" wcet="14271">
		<resources />
	</task>
</taskset>
"""

class PFPSpinlockInflationVsILP3(unittest.TestCase):
    def setUp(self):
        xml = StringIO.StringIO(TASKSET)
        self.ts = ser.load(xml)

        lb.assign_fp_preemption_levels(self.ts)

    @unittest.skipIf(not schedcat.locking.bounds.lp_cpp_available, "no native LP solver available")
    def test_ilp_dominance(self):
        "check that the ILP analysis is at least as accurate as inflation"
        (res_inf, ts_inf) = pfp_sched_test_msrp_inflate(self.ts)
        (res_ilp, ts_ilp) = pfp_sched_test_msrp_ilp(self.ts)

        self.assertTrue(res_inf)
        self.assertTrue(res_ilp)
        for i in range(len(self.ts)):
            self.assertGreaterEqual(ts_inf[i].response_time, ts_ilp[i].response_time)



def generate_taskset(num_cpus=4, util_per_cpu=0.5, tasks_per_cpu=3, req_prob=0.5,
                     min_req_len=50, max_req_len=250):
    ts = emstada.gen_taskset('uni-broad', 'logunif', num_cpus * tasks_per_cpu,
                             num_cpus * util_per_cpu)

    ts.sort_by_period()
    ts.assign_ids()
    r.initialize_resource_model(ts)
    lb.assign_fp_preemption_levels(ts)

    for t in ts:
        # randomly partition
#         t.partition = random.randint(0, num_cpus - 1)
        # randomly assign requests
        if random.random() < req_prob:
            t.resmodel[0].add_write_request(random.randint(min_req_len, max_req_len))

    bins = worst_fit_decreasing(ts, num_cpus, weight=lambda t: t.utilization())
    for cpu, tasks in enumerate(bins):
        for t in tasks:
            t.partition = cpu

    return ts

ts_count = 0
schedulable_count = 0

def generate_and_compare(*args, **kargs):
    ts = generate_taskset(*args, **kargs)
    (res_inf, ts_inf) = pfp_sched_test_msrp_inflate(ts)
    (res_ilp, ts_ilp) = pfp_sched_test_msrp_ilp(ts)

    global ts_count, schedulable_count

    ts_count += 1
    if res_ilp:
        schedulable_count +=1

    keep = False
    if not res_ilp and res_inf:
        # ILP should not say 'no' when inflation-based analysis says 'yes'
        print 'TS%d - FAIL: ILP -> no, but INF -> yes' % ts_count
        keep = True

    if res_ilp and res_inf:
        for i in range(len(ts)):
            if ts_inf[i].response_time < ts_ilp[i].response_time:
                # ILP should not never be more pessimistic
                print 'TS%d T%d: ILP -> %d, but INF -> %d' % \
                    (ts_count, i + 1, ts_inf[i].response_time, ts_ilp[i].response_time)
                keep = True

    if keep:
        ser.write(ts, "ts-%07d.xml" % ts_count)



if __name__ == '__main__':
    while True:
        generate_and_compare(4, 0.3, 4, 0.5)
        if ts_count % 100 == 0:
            print "C: %6d S: %6d -> %5.2f" % (ts_count, schedulable_count, (schedulable_count / ts_count) * 100)
