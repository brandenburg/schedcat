from __future__ import division

from schedcat.util.csv import load_columns as load_column_csv
from schedcat.util.math import monotonic_pwlin, const

class Overheads(object):
    """Legacy overhead objects"""
    def __init__(self):
        self.quantum_length = 1000 # microseconds
        self.zero_overheads()

    FIELD_MAPPING = [
        # scheduling-related overheads
        ('IPI-LATENCY',     'ipi_latency'),
        ('SCHEDULE',        'schedule'),
        ('RELEASE',         'release'),
        ('CXS',             'ctx_switch'),
        ('TICK',            'tick'),
        ('RELEASE-LATENCY', 'release_latency'),

        # locking- and system-call-related overheads
        ('LOCK',            'lock'),
        ('UNLOCK',          'unlock'),
        ('READ-LOCK',       'read_lock'),
        ('READ-UNLOCK',     'read_unlock'),
        ('SYSCALL-IN',      'syscall_in'),
        ('SYSCALL-OUT',     'syscall_out'),
        ]

    def zero_overheads(self):
        # cache-related preemption/migration delay
        self.cache_affinity_loss = CacheDelay()
        # cost of loading working set into cache at start of execution
        self.initial_cache_load = CacheDelay()
        for (name, field) in self.FIELD_MAPPING:
            self.__dict__[field] = const(0)

    def __str__(self):
        return " ".join(["%s: %s" % (name, self.__dict__[field])
                         for (name, field) in Overheads.FIELD_MAPPING])

    def load_approximations(self, fname, non_decreasing=True):
        data = load_column_csv(fname, convert=float)
        if not 'TASK-COUNT' in data.by_name:
            raise IOError, "TASK-COUNT column is missing"

        for (name, field) in Overheads.FIELD_MAPPING:
            if name in data.by_name:
                points = zip(data.by_name['TASK-COUNT'], data.by_name[name])
                if non_decreasing:
                    self.__dict__[field] = monotonic_pwlin(points)
                else:
                    self.__dict__[field] = piece_wise_linear(points)

    @staticmethod
    def from_file(fname, non_decreasing=True):
        o = Overheads()
        o.source = fname
        o.load_approximations(fname, non_decreasing)
        return o

class CacheDelay(object):
    """Cache-related Preemption and Migration Delay (CPMD)
    Overheads are expressed as a piece-wise linear function of working set size.
    """

    MEM, L1, L2, L3 = 0, 1, 2, 3
    MAPPING = list(enumerate(["MEM", "L1", "L2", "L3"]))

    def __init__(self, l1=0, l2=0, l3=0, mem=0):
        self.mem_hierarchy  = [const(mem), const(l1), const(l2), const(l3)]
        for (i, name) in CacheDelay.MAPPING:
            self.__dict__[name] = self.mem_hierarchy[i]

    def cpmd_cost(self, shared_mem_level, working_set_size):
        return self.mem_hierarchy[shared_mem_level](working_set_size)

    def set_cpmd_cost(self, shared_mem_level, approximation):
        self.mem_hierarchy[shared_mem_level] = approximation
        name = CacheDelay.MAPPING[shared_mem_level][1]
        self.__dict__[name] = self.mem_hierarchy[shared_mem_level]

    def max_cost(self, working_set_size):
        return max([f(working_set_size) for f in self.mem_hierarchy])

    def __call__(self, wss):
        return self.max_cost(wss)

    @staticmethod
    def get_idx_for_name(key):
        for (i, name) in CacheDelay.MAPPING:
            if name == key:
                return i
        assert False # bad key

    @staticmethod
    def from_file(fname, non_decreasing=True):
        data = load_column_csv(fname, convert=float)
        if not 'WSS' in data.by_name:
            raise IOError, 'WSS column is missing'

        o = CacheDelay()

        for idx, name in CacheDelay.MAPPING:
            if name in data.by_name:
                points = zip(data.by_name['WSS'], data.by_name[name])
                if non_decreasing:
                    o.mem_hierarchy[idx] = monotonic_pwlin(points)
                else:
                    o.mem_hierarchy[idx] = piece_wise_linear(points)
                o.__dict__[name] = o.mem_hierarchy[idx]
        return o
