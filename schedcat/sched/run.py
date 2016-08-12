from __future__ import division

from schedcat.mapping.binpack import best_fit_decreasing
from math import ceil
from fractions import Fraction

class Server(object):

    def __init__(self, rate, deadlines=None, clients=None, id=None,
                 created_from=None):
        self.rate = rate
        if deadlines is None:
            deadlines = frozenset()
        self.deadlines = deadlines
        if clients is None:
            clients = []
        self.clients = clients
        self.id = id
        self.created_from = created_from

    @staticmethod
    def aggregate(servers, id=None):
        total_rate = sum((s.rate for s in servers))
        deadlines = set()
        for s in servers:
            deadlines |= s.deadlines
        return Server(total_rate, deadlines=frozenset(deadlines),
                      clients=list(servers), id=id)

    def dual(self):
        return Server(1 - self.rate,
                      deadlines=self.deadlines,
                      clients=self.clients,
                      id='%s*' % self.id if self.id else None,
                      created_from=self)

    def is_unit_server(self):
        return self.rate == 1

    def is_null_server(self):
        return self.rate == 0

    def __repr__(self):
        return 'Server(%s%s%s%s)' % \
            (self.rate,
             ', deadlines=%s' % self.deadlines if self.deadlines else '',
             ', clients=%s' % self.clients if self.clients else '',
             ', id=%s' % self.id if self.id else '')

    def __str__(self):
        return repr(self)

def find_packing(servers):
    return best_fit_decreasing(servers, 0, weight=lambda s: s.rate)

def dual(servers):
    return [s.dual() for s in servers]

def pack(packing, next_id=None):
    if not next_id:
        return [Server.aggregate(bin) for bin in packing]
    else:
        return [Server.aggregate(bin, next_id + i)
                for (i, bin) in enumerate(packing)]

def reduction_step(servers, next_id=None):
    packing = find_packing(servers)
    packed = pack(packing, next_id)
    return dual(packed)

def total_rate(servers):
    return sum((s.rate for s in servers))

def all_unit_servers(servers):
    return all((s.is_unit_server() for s in servers))

def ensure_integer_rate(servers):
    r = total_rate(servers)
    next_int = Fraction(ceil(r))
    if next_int != r:
        return list(servers) + [Server(next_int - r, id='slack')]
    else:
        return list(servers)

def reduce(server, next_id=None):
    levels = [ensure_integer_rate(server)]
    while True:
        packing = find_packing(levels[-1])
        packed = pack(packing, next_id)
        if all_unit_servers(packed):
            # great, we are done
            return packed, levels
        else:
            # nope, need to look at duals and continue
            levels.append(dual(packed))
        if next_id:
            next_id += len(packed)

def max_number_of_preemptions_per_job_release(levels):
    # Lemma 8 in the RUN journal paper
    return int(ceil(len(levels) / 2))

