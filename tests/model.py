from __future__ import division

import unittest
from StringIO import StringIO
from fractions import Fraction

import schedcat.model.tasks as m
import schedcat.model.serialize as s
import schedcat.model.resources as r

class Tasks(unittest.TestCase):
    def setUp(self):
        self.t1 = m.SporadicTask(10, 100)
        self.t2 = m.SporadicTask(5, 19, 15, id=3)
        self.t3 = m.SporadicTask(25, 50, id=5, deadline=75)

    def test_deadline_type(self):
        self.assertTrue(self.t1.implicit_deadline())
        self.assertFalse(self.t2.implicit_deadline())
        self.assertFalse(self.t3.implicit_deadline())

        self.assertTrue(self.t1.constrained_deadline())
        self.assertTrue(self.t2.constrained_deadline())
        self.assertFalse(self.t3.implicit_deadline())

    def test_utilization(self):
        self.assertEqual(self.t1.utilization_q(), Fraction(10, 100))
        self.assertEqual(self.t1.utilization(), 0.1)

    def test_density(self):
        self.assertEqual(self.t2.density_q(), Fraction(1, 3))
        self.assertEqual(self.t2.density(), 1/3)

        self.assertEqual(self.t3.density_q(), Fraction(1, 2))
        self.assertEqual(self.t3.density(), 0.5)

    def test_repr(self):
        e1 = eval("m." + repr(self.t1))
        self.assertEqual(e1.cost, self.t1.cost)
        self.assertEqual(e1.period, self.t1.period)
        self.assertEqual(e1.deadline, self.t1.deadline)
        self.assertEqual(e1.id, self.t1.id)

        e2 = eval("m." + repr(self.t2))
        self.assertEqual(e2.cost, self.t2.cost)
        self.assertEqual(e2.period, self.t2.period)
        self.assertEqual(e2.deadline, self.t2.deadline)
        self.assertEqual(e2.id, self.t2.id)

        e3 = eval("m." + repr(self.t3))
        self.assertEqual(e3.cost, self.t3.cost)
        self.assertEqual(e3.period, self.t3.period)
        self.assertEqual(e3.deadline, self.t3.deadline)
        self.assertEqual(e3.id, self.t3.id)

    def test_maxjobs(self):
        t = m.SporadicTask(None, 2)
        t.response_time = 6.6
        self.assertEqual(t.maxjobs(7.25), 7)

    def test_tardiness(self):
        t = m.SporadicTask(None, 2)
        t.response_time = 1.5
        self.assertEqual(t.tardiness(), 0)
        t.response_time = 6
        self.assertEqual(t.tardiness(), 4)

# TODO: Write tests for TaskSystem

class Tasks(unittest.TestCase):
    def setUp(self):
        pass

    def test_property(self):
        req = r.ResourceRequirement(1, 2, 10, 13, 4)
        self.assertEqual(req.max_reads, 13)
        self.assertEqual(req.max_writes, 2)
        self.assertEqual(req.max_requests, 15)
        self.assertEqual(req.max_write_length, 10)
        self.assertEqual(req.max_read_length,   4)
        self.assertEqual(req.max_length,       10)

    def test_copy(self):
        ts = m.TaskSystem([
            m.SporadicTask(10, 100),
        ])
        r.initialize_resource_model(ts)
        ts[0].resmodel[0].add_request(10)

        ts2 = ts.copy()
        ts[0].resmodel[0].add_request(30)

        self.assertEqual(ts[0].resmodel[0].max_length, 30)
        self.assertEqual(ts[0].resmodel[0].max_requests, 2)

        self.assertEqual(ts2[0].resmodel[0].max_length, 10)
        self.assertEqual(ts2[0].resmodel[0].max_requests, 1)


class Serialization(unittest.TestCase):
    def setUp(self):
        self.t1 = m.SporadicTask(10, 100)
        self.t2 = m.SporadicTask(5, 19, 15, id=3)
        self.t3 = m.SporadicTask(25, 50, id=5, deadline=75)
        self.ts = m.TaskSystem([self.t1, self.t2, self.t3])
        self.f  = StringIO()

    def test_serialize_task(self):
        for t in self.ts:
            s.write_xml(s.task(t), self.f)
            self.f.seek(0)
            x = s.load(self.f)
            self.assertIsInstance(x, m.SporadicTask)
            self.assertEqual(x.cost, t.cost)
            self.assertEqual(x.deadline, t.deadline)
            self.assertEqual(x.period, t.period)
            self.assertEqual(x.id, t.id)
            self.f.seek(0)
            self.f.truncate()

    def test_serialize_taskset(self):
        s.write(self.ts, self.f)
        self.f.seek(0)
        xs = s.load(self.f)
        self.assertIsInstance(xs, m.TaskSystem)
        self.assertEqual(len(xs), len(self.ts))
        for x,t in zip(xs, self.ts):
            self.assertEqual(x.cost, t.cost)
            self.assertEqual(x.deadline, t.deadline)
            self.assertEqual(x.period, t.period)
            self.assertEqual(x.id, t.id)

    def test_serialize_resmodel(self):
        r.initialize_resource_model(self.ts)
        self.t1.resmodel[1].add_request(1)
        self.t2.resmodel[1].add_read_request(2)
        self.t2.resmodel['serial I/O'].add_request(2)
        self.t3.resmodel['serial I/O'].add_request(3)

        for t in self.ts:
            s.write_xml(s.task(t), self.f)
            self.f.seek(0)
            x = s.load(self.f)
            self.assertIsInstance(x.resmodel, r.ResourceRequirements)
            self.assertEqual(len(x.resmodel), len(t.resmodel))
            self.assertEqual(x.resmodel.keys(), t.resmodel.keys())
            for res_id in x.resmodel:
                self.assertEqual(x.resmodel[res_id].max_reads, t.resmodel[res_id].max_reads)
                self.assertEqual(x.resmodel[res_id].max_writes, t.resmodel[res_id].max_writes)
                self.assertEqual(x.resmodel[res_id].max_requests, t.resmodel[res_id].max_requests)
                self.assertEqual(x.resmodel[res_id].max_read_length, t.resmodel[res_id].max_read_length)
                self.assertEqual(x.resmodel[res_id].max_write_length, t.resmodel[res_id].max_write_length)
                self.assertEqual(x.resmodel[res_id].max_length, t.resmodel[res_id].max_length)
            self.f.seek(0)
            self.f.truncate()
