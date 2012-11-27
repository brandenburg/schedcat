from __future__ import division

import unittest
import StringIO
from math import ceil

import schedcat.overheads.model as m
import schedcat.overheads.jlfp as jlfp
import schedcat.overheads.pfair as pfair
import schedcat.overheads.fp as fp
import schedcat.overheads.locking as locking
import schedcat.model.tasks as tasks
import schedcat.model.resources as res

from schedcat.util.math import const

class Model(unittest.TestCase):
    def setUp(self):
        s = """TASK-COUNT, SCHEDULE
                    10   , 10
                    20   , 20
                    30   , 20
                    40   , 17
                    50   , 40
"""
        self.sched_file = StringIO.StringIO(s)

        s = """WSS, MEM, L3
1024,   100, 50
2048,   200, 50
4096,   400, 400
16384, 1600, 17000
"""
        self.cpmd_file = StringIO.StringIO(s)

    def test_init(self):
        o = m.Overheads()
        self.assertEqual(o.schedule(10), 0)
        self.assertEqual(o.ctx_switch(10), 0)
        self.assertEqual(o.quantum_length, 1000)

    def test_from_file(self):
        o = m.Overheads.from_file(self.sched_file)
        self.assertIsInstance(o.schedule(10), float)
        self.assertAlmostEqual(o.schedule(10), 10.0)
        self.assertAlmostEqual(o.schedule(5), 5.0)
        self.assertAlmostEqual(o.schedule(15), 15.0)
        self.assertAlmostEqual(o.schedule(20), 20.0)
        self.assertAlmostEqual(o.schedule(25), 20.0)
        self.assertAlmostEqual(o.schedule(30), 20.0)
        self.assertAlmostEqual(o.schedule(40), 20.0)
        self.assertAlmostEqual(o.schedule(45), 30.0)

    def test_cpmd_from_file(self):
        o = m.CacheDelay.from_file(self.cpmd_file)
        self.assertIsInstance(o.MEM(10), float)
        self.assertIsInstance(o(10), float)
        self.assertAlmostEqual(o(1024), 100.0)
        self.assertAlmostEqual(o.MEM(8192), 800.0)
        self.assertGreater(o(8192), 800.0)
        self.assertAlmostEqual(o(16384), 17000.0)

    def test_exceptions(self):
        self.assertRaises(IOError, m.Overheads.from_file, '/non/existant')
        self.assertRaises(IOError, m.CacheDelay.from_file, '/non/existant')
        self.assertRaises(IOError, m.Overheads.from_file, self.cpmd_file)
        self.assertRaises(IOError, m.CacheDelay.from_file, self.sched_file)


class JLFPOverheads(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                        tasks.SporadicTask(10000, 100000),
                        tasks.SporadicTask( 5000,  50000),
                        ])
        for t in self.ts:
            t.wss = 0
        self.o = m.Overheads()

    def unchanged_period(self):
        self.assertEqual(self.ts[0].period, 100000)
        self.assertEqual(self.ts[1].period,  50000)

    def unchanged_deadline(self):
        self.assertEqual(self.ts[0].deadline, 100000)
        self.assertEqual(self.ts[1].deadline,  50000)

    def unchanged_cost(self):
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)

    def test_none(self):
        self.assertEqual(jlfp.charge_scheduling_overheads(None, 4,  False, self.ts), self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()

    def test_initial_load(self):
        self.o.initial_cache_load = const(4)
        self.assertEqual(jlfp.charge_initial_load(self.o, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10004)
        self.assertEqual(self.ts[1].cost,  5004)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_sched(self):
        self.o.schedule = const(2)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10004)
        self.assertEqual(self.ts[1].cost,  5004)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_ctx_switch(self):
        self.o.ctx_switch = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10002)
        self.assertEqual(self.ts[1].cost,  5002)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_cache_affinity_loss(self):
        self.o.cache_affinity_loss = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10001)
        self.assertEqual(self.ts[1].cost,  5001)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_ipi_latency(self):
        self.o.ipi_latency = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10001)
        self.assertEqual(self.ts[1].cost,  5001)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_release_latency(self):
        self.o.release_latency = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.unchanged_cost()
        self.assertEqual(self.ts[0].period, 99999)
        self.assertEqual(self.ts[1].period, 49999)
        self.assertEqual(self.ts[0].deadline, 99999)
        self.assertEqual(self.ts[1].deadline, 49999)

    def test_tick(self):
        self.o.tick = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10013)
        self.assertEqual(self.ts[1].cost,  5008)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_release(self):
        self.o.release = const(1)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10005)
        self.assertEqual(self.ts[1].cost,  5005)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_dedicated(self):
        self.o.ipi_latency = const(100)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  True, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10100)
        self.assertEqual(self.ts[1].cost,  5100)
        self.unchanged_period()
        self.unchanged_deadline()

        self.o.release = const(133)
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 4,  True, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10333)
        self.assertEqual(self.ts[1].cost,  5333)

        self.unchanged_period()
        self.unchanged_deadline()


    def test_tick_example(self):
        e1  = 2000
        e2  = 3000
        Q   = 5000
        tck = 2000
        self.ts[0].cost = e1
        self.ts[1].cost = e2
        self.o.tick = const(tck)
        self.o.quantum_length = Q
        self.assertEqual(jlfp.charge_scheduling_overheads(self.o, 1,  False, self.ts), self.ts)
        self.assertEqual(jlfp.quantize_params(self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, int(ceil(30000 / 3)))
        self.assertEqual(self.ts[1].cost, 5000 + int(ceil(20000 / 3)))

class PfairOverheads(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                        tasks.SporadicTask(9995, 100000),
                        tasks.SporadicTask( 4995,  50000),
                        ])
        for t in self.ts:
            t.wss = 0
        self.o = m.Overheads()
        self.o.quantum_length = 500

    def unchanged_period(self):
        # not strictly unchanged, but affected by a quantum release delay
        self.assertEqual(self.ts[0].period, 99500)
        self.assertEqual(self.ts[1].period, 49500)

    def unchanged_deadline(self):
        # not strictly unchanged, but affected by a quantum release delay
        self.assertEqual(self.ts[0].deadline, 99500)
        self.assertEqual(self.ts[1].deadline,  49500)

    def unchanged_cost(self):
        # not strictly unchanged, but only quantized
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)

    def test_none(self):
        self.assertEqual(pfair.charge_scheduling_overheads(None, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 9995)
        self.assertEqual(self.ts[1].cost,  4995)
        self.assertEqual(self.ts[0].period, 100000)
        self.assertEqual(self.ts[1].period, 50000)
        self.assertEqual(self.ts[0].deadline, 100000)
        self.assertEqual(self.ts[1].deadline, 50000)

    def test_quant(self):
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_periodic(self):
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts,
                                                           aligned_periodic_releases=True), self.ts)
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)
        self.assertEqual(self.ts[0].deadline, 100000)
        self.assertEqual(self.ts[1].deadline,  50000)
        self.assertEqual(self.ts[0].period, 100000)
        self.assertEqual(self.ts[1].period, 50000)

    def test_sched(self):
        self.o.schedule = const(50)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 11500)
        self.assertEqual(self.ts[1].cost,  6000)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_ctx_switch(self):
        self.o.ctx_switch = const(100)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 12500)
        self.assertEqual(self.ts[1].cost,  6500)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_cache_affinity_loss(self):
        self.o.cache_affinity_loss = const(0.5)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10500)
        self.assertEqual(self.ts[1].cost,  5000)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_ipi_latency(self):
        # IPI latency is irrelevant for Pfair
        self.o.ipi_latency = const(1000)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_release_latency(self):
        self.o.release_latency = const(100)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 12500)
        self.assertEqual(self.ts[1].cost,  6500)
        self.assertEqual(self.ts[0].period, 99000)
        self.assertEqual(self.ts[1].period, 49000)
        self.assertEqual(self.ts[0].deadline, 99000)
        self.assertEqual(self.ts[1].deadline, 49000)

    def test_tick(self):
        self.o.tick = const(50)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 11500)
        self.assertEqual(self.ts[1].cost,  6000)
        self.unchanged_period()
        self.unchanged_deadline()

    def test_release(self):
        self.o.release = const(50)
        self.assertEqual(pfair.charge_scheduling_overheads(self.o, 4,  False, self.ts), self.ts)
        self.assertEqual(self.ts[0].cost, 11500)
        self.assertEqual(self.ts[1].cost,  6000)
        self.assertEqual(self.ts[0].period, 99000)
        self.assertEqual(self.ts[1].period, 49000)
        self.assertEqual(self.ts[0].deadline, 99000)
        self.assertEqual(self.ts[1].deadline, 49000)


class FPOverheads(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                        tasks.SporadicTask(10000, 100000),
                        tasks.SporadicTask( 5000,  50000),
                        ])
        for t in self.ts:
            t.wss = 0
        self.o = m.Overheads()

    def unchanged_period(self):
        self.assertEqual(self.ts[0].period, 100000)
        self.assertEqual(self.ts[1].period,  50000)

    def unchanged_deadline(self):
        self.assertEqual(self.ts[0].deadline, 100000)
        self.assertEqual(self.ts[1].deadline,  50000)

    def unchanged_cost(self):
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)

    def no_jitter(self):
        self.assertEqual(self.ts[0].jitter, 0)
        self.assertEqual(self.ts[1].jitter, 0)

    def test_none(self):
        ts = fp.charge_scheduling_overheads(None, 4,  False, self.ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()

    def test_sched(self):
        self.o.schedule = const(2)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.assertEqual(self.ts[0].cost, 10004)
        self.assertEqual(self.ts[1].cost,  5004)
        self.unchanged_period()
        self.unchanged_deadline()
        self.no_jitter()

    def test_ctx_switch(self):
        self.o.ctx_switch = const(1)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.assertEqual(self.ts[0].cost, 10002)
        self.assertEqual(self.ts[1].cost,  5002)
        self.unchanged_period()
        self.unchanged_deadline()
        self.no_jitter()

    def test_cache_affinity_loss(self):
        self.o.cache_affinity_loss = const(1)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.assertEqual(self.ts[0].cost, 10001)
        self.assertEqual(self.ts[1].cost,  5001)
        self.unchanged_period()
        self.unchanged_deadline()
        self.no_jitter()

    def test_ipi_latency(self):
        self.o.ipi_latency = const(1)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()
        self.no_jitter()

        ts = fp.charge_scheduling_overheads(self.o, 4,  True, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()
        self.assertEqual(self.ts[0].jitter, 1)
        self.assertEqual(self.ts[1].jitter, 1)

    def test_release_latency(self):
        self.o.release_latency = const(1)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()
        self.assertEqual(self.ts[0].jitter, 1)
        self.assertEqual(self.ts[1].jitter, 1)

    def test_tick(self):
        self.o.tick = const(123)
        self.o.quantum_length = 777
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()
        self.no_jitter()
        self.assertEqual(ts[0].cost, 123)
        self.assertEqual(ts[0].period, 777)

    def test_release(self):
        self.o.release = const(17)
        ts = fp.charge_scheduling_overheads(self.o, 4,  False, self.ts)
        self.assertEqual(fp.quantize_params(ts), ts)
        self.assertIsNot(ts, False)
        self.assertIsNot(ts, self.ts)
        self.unchanged_cost()
        self.unchanged_period()
        self.unchanged_deadline()
        self.assertEqual(self.ts[0].jitter, 17)
        self.assertEqual(self.ts[1].jitter, 17)
        self.assertEqual(ts[0].cost, 17)
        self.assertEqual(ts[0].jitter, 0)
        self.assertEqual(ts[0].period, self.ts[0].period)
        self.assertEqual(ts[1].cost, 17)
        self.assertEqual(ts[1].jitter, 0)
        self.assertEqual(ts[1].period, self.ts[1].period)


class LockingOverheads(unittest.TestCase):
    def setUp(self):
        self.ts = tasks.TaskSystem([
                        tasks.SporadicTask(10000, 100000),
                        tasks.SporadicTask( 5000,  50000),
                        ])
        for t in self.ts:
            t.wss = 0
        res.initialize_resource_model(self.ts)
        self.ts[0].resmodel[0].add_request(11)
        self.ts[0].resmodel[0].add_request(7)
        self.ts[1].resmodel[0].add_request(17)

        self.ts[0].resmodel[1].add_read_request(11)
        self.ts[0].resmodel[1].add_read_request(1)
        self.ts[1].resmodel[1].add_read_request(17)

        self.ts[0].resmodel[2].add_read_request(11)
        self.ts[0].resmodel[2].add_read_request(1)
        self.ts[1].resmodel[2].add_write_request(17)
        self.ts[1].resmodel[2].add_write_request(1)

        self.o = m.Overheads()

    def no_reads(self):
        for t in self.ts:
            for res_id in t.resmodel:
                req = t.resmodel[res_id]
                req.convert_reads_to_writes()

    def init_susp(self):
        for t in self.ts:
            t.suspended = 0

    def not_lossy(self):
        self.assertIs(self.ts[0].resmodel[0].max_reads, 0)
        self.assertIs(self.ts[0].resmodel[0].max_writes, 2)
        self.assertIs(self.ts[1].resmodel[0].max_reads, 0)
        self.assertIs(self.ts[1].resmodel[0].max_writes, 1)

        self.assertIs(self.ts[0].resmodel[1].max_reads, 2)
        self.assertIs(self.ts[0].resmodel[1].max_writes, 0)
        self.assertIs(self.ts[1].resmodel[1].max_reads, 1)
        self.assertIs(self.ts[1].resmodel[1].max_writes, 0)

        self.assertIs(self.ts[0].resmodel[2].max_reads, 2)
        self.assertIs(self.ts[0].resmodel[2].max_writes, 0)
        self.assertIs(self.ts[1].resmodel[2].max_reads, 0)
        self.assertIs(self.ts[1].resmodel[2].max_writes, 2)

    def not_lossy_no_reads(self):
        self.assertIs(self.ts[0].resmodel[0].max_reads, 0)
        self.assertIs(self.ts[0].resmodel[0].max_writes, 2)
        self.assertIs(self.ts[1].resmodel[0].max_reads, 0)
        self.assertIs(self.ts[1].resmodel[0].max_writes, 1)

        self.assertIs(self.ts[0].resmodel[1].max_reads, 0)
        self.assertIs(self.ts[0].resmodel[1].max_writes, 2)
        self.assertIs(self.ts[1].resmodel[1].max_reads, 0)
        self.assertIs(self.ts[1].resmodel[1].max_writes, 1)

        self.assertIs(self.ts[0].resmodel[2].max_reads,  0)
        self.assertIs(self.ts[0].resmodel[2].max_writes, 2)
        self.assertIs(self.ts[1].resmodel[2].max_reads, 0)
        self.assertIs(self.ts[1].resmodel[2].max_writes, 2)

    def unchanged_period(self):
        self.assertEqual(self.ts[0].period, 100000)
        self.assertEqual(self.ts[1].period,  50000)

    def unchanged_deadline(self):
        self.assertEqual(self.ts[0].deadline, 100000)
        self.assertEqual(self.ts[1].deadline,  50000)

    def unchanged_cost(self):
        self.assertEqual(self.ts[0].cost, 10000)
        self.assertEqual(self.ts[1].cost,  5000)

    def unchanged_resmodel(self):
        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  11)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  0)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,  17)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,  0)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,  11)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  0)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17)

    def unchanged_resmodel_no_reads(self):
        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length, 11)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length, 17)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length, 11)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17)


    def oheads(self, sched=False):
        self.o.lock = const(3)
        self.o.unlock = const(5)
        self.o.read_lock = const(7)
        self.o.read_unlock = const(11)
        self.o.syscall_in = const(17)
        self.o.syscall_out = const(19)

        if sched:
            self.o.ipi_latency = const(23)
            self.o.schedule    = const(27)
            self.o.ctx_switch  = const(31)
            self.o.cache_affinity_loss.set_cpmd_cost(m.CacheDelay.L1, const(41))

#  31     37     41     43     47     53     59     61     67     71

#         ('LOCK',            'lock'),
#         ('UNLOCK',          'unlock'),
#         ('READ-LOCK',       'read_lock'),
#         ('READ-UNLOCK',     'read_unlock'),
#         ('SYSCALL-IN',      'syscall_in'),
#         ('SYSCALL-OUT',     'syscall_out'),

    def test_spinlock_none(self):
        self.assertIs(locking.charge_spinlock_overheads(None, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy()
        self.unchanged_resmodel()

    def test_spinlock_zero(self):
        self.assertIs(locking.charge_spinlock_overheads(self.o, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy()
        self.unchanged_resmodel()

    def test_spinlock_infeasible(self):
        self.o.syscall_in = const(10000000)
        self.assertIs(locking.charge_spinlock_overheads(self.o, self.ts), False)

    def test_spinlock_integral(self):
        self.o.lock = const(1.75)
        self.assertIs(locking.charge_spinlock_overheads(self.o, self.ts), self.ts)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + 2)

    def test_spinlock(self):
        self.oheads()
        self.assertIs(locking.charge_spinlock_overheads(self.o, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy()

        scost = 17 + 19
        rcost = 7 + 11
        wcost = 3 + 5

        self.assertEqual(self.ts[0].cost, 10000 + 2 * wcost + 4 * rcost + 6 * scost)
        self.assertEqual(self.ts[1].cost,  5000 + 3 * wcost + 1 * rcost + 4 * scost)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + wcost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + wcost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  11 + rcost)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  0)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,  17 + rcost)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,  0)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,  11 + rcost)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  0)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + wcost)

    def test_sem_none(self):
        self.assertIs(locking.charge_semaphore_overheads(None, True, True, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy()
        self.unchanged_resmodel()

    def test_sem_zero(self):
        self.no_reads()
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, False, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy_no_reads()
        self.unchanged_resmodel_no_reads()

    def test_sem_infeasible(self):
        self.no_reads()
        self.o.syscall_in = const(10000000)
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, False, self.ts), False)

    def test_sem_integral(self):
        self.no_reads()
        self.o.unlock = const(1.75)
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, False, self.ts), self.ts)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + 2)

    def test_sem_lock_only(self):
        self.oheads()
        self.no_reads()
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, False, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        scost = 17 + 19
        xcost = 3 + 5
        ocost = 5

        self.assertEqual(self.ts[0].cost, 10000 + 6 * xcost + 12 * scost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * xcost +  8 * scost)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + scost + ocost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + scost + ocost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + scost + ocost)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + scost + ocost)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + scost + ocost)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + scost + ocost)

    def test_sem_lock_and_sched(self):
        self.oheads(sched=True)
        self.no_reads()
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, False, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        ecost = 2 * 17 + 2 * 19 + 3 + 5 + 3 * 27 + 3 * 31 + 2 * 41 + 23
        xcost = 2 * 27 + 2 * 31 + 17 + 19 + 5 + 23

        self.assertEqual(self.ts[0].cost, 10000 + 6 * ecost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * ecost)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + xcost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + xcost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + xcost)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + xcost)

    def test_sem_lock_and_sched_saw(self):
        self.oheads(sched=True)
        self.no_reads()
        self.init_susp()
        self.assertIs(locking.charge_semaphore_overheads(self.o, True, True, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        ecost = 2 * 17 + 2 * 19 + 3 + 5 + 3 * 27 + 3 * 31 + 2 * 41
        xcost = 2 * 27 + 2 * 31 + 17 + 19 + 5 + 23
        esusp = 23

        self.assertEqual(self.ts[0].cost, 10000 + 6 * ecost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * ecost)

        self.assertEqual(self.ts[0].suspended, 6 * esusp)
        self.assertEqual(self.ts[1].suspended, 4 * esusp)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + xcost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + xcost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + xcost)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + xcost)

    def test_sem_lock_and_sched_np(self):
        self.oheads(sched=True)
        self.no_reads()
        self.assertIs(locking.charge_semaphore_overheads(self.o, False, False, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        ecost = 2 * 17 + 2 * 19 + 3 + 5 + 3 * 27 + 3 * 31 + 2 * 41 + 23
        xcost = 1 * 27 + 1 * 31 + 17 + 19 + 5 + 23
        xcost_local = xcost + 27 + 31 # additional scheduler invocation

        self.assertEqual(self.ts[0].cost, 10000 + 6 * ecost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * ecost)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + xcost)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length_local, 11 + xcost_local)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + xcost)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length_local, 17 + xcost_local)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length_local,  11 + xcost_local)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + xcost)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length_local,  11 + xcost_local)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length_local,  11 + xcost_local)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + xcost)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length_local,  11 + xcost_local)

    def test_dpcp_none(self):
        self.assertIs(locking.charge_dpcp_overheads(None, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy()
        self.unchanged_resmodel()

    def test_dpcp_zero(self):
        self.no_reads()
        self.init_susp()
        self.assertIs(locking.charge_dpcp_overheads(self.o, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.unchanged_cost()
        self.not_lossy_no_reads()
        self.unchanged_resmodel_no_reads()

    def test_dpcp_infeasible(self):
        self.no_reads()
        self.init_susp()
        self.o.syscall_in = const(10000000)
        self.assertIs(locking.charge_dpcp_overheads(self.o, self.ts), False)

    def test_dpcp_integral(self):
        self.no_reads()
        self.init_susp()
        self.o.lock = const(1.75)
        self.assertIs(locking.charge_dpcp_overheads(self.o, self.ts), self.ts)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + 2)

    def test_dpcp_lock_only(self):
        self.oheads()
        self.no_reads()
        self.init_susp()
        self.assertIs(locking.charge_dpcp_overheads(self.o, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        ecost = 17 + 19
        xcost = 17 + 19 + 3 + 5
        esusp = 0 + xcost

        self.assertEqual(self.ts[0].cost, 10000 + 6 * ecost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * ecost)

        self.assertEqual(self.ts[0].suspended, 6 * esusp)
        self.assertEqual(self.ts[1].suspended, 4 * esusp)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + xcost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + xcost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + xcost)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + xcost)

    def test_dpcp_and_sched(self):
        self.oheads(sched=True)
        self.no_reads()
        self.init_susp()
        self.assertIs(locking.charge_dpcp_overheads(self.o, self.ts), self.ts)
        self.unchanged_period()
        self.unchanged_deadline()
        self.not_lossy_no_reads()

        ecost = 17 + 19 + 2 * (27 + 31 + 41)
        xcost = 17 + 19 + 3 + 5 + 3 * (27 + 31)
        esusp = 2 * 23 + xcost

        self.assertEqual(self.ts[0].cost, 10000 + 6 * ecost)
        self.assertEqual(self.ts[1].cost,  5000 + 4 * ecost)

        self.assertEqual(self.ts[0].suspended, 6 * esusp)
        self.assertEqual(self.ts[1].suspended, 4 * esusp)

        self.assertEqual(self.ts[0].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[0].max_write_length, 11 + xcost)
        self.assertEqual(self.ts[1].resmodel[0].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[0].max_write_length, 17 + xcost)

        self.assertEqual(self.ts[0].resmodel[1].max_read_length,  0)
        self.assertEqual(self.ts[0].resmodel[1].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[1].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[1].max_write_length,   17 + xcost)

        self.assertEqual(self.ts[0].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[0].resmodel[2].max_write_length,  11 + xcost)
        self.assertEqual(self.ts[1].resmodel[2].max_read_length,   0)
        self.assertEqual(self.ts[1].resmodel[2].max_write_length, 17 + xcost)
