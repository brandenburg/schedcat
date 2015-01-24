from collections import defaultdict
from itertools import chain

class ResourceRequirement(object):
    def __init__(self, res_id, num_writes=1, write_length=1,
                 num_reads=0, read_length=0, priority=0):
        self.res_id           = res_id
        self.max_writes       = num_writes
        self.max_reads        = num_reads
        self.max_write_length = write_length
        self.max_read_length  = read_length
        self.priority         = priority

    @property
    def max_requests(self):
        "Number of requests of any kind."
        return self.max_writes + self.max_reads

    @property
    def max_length(self):
        "Maximum request length (of any kind)."
        return max(self.max_write_length, self.max_read_length)

    def add_request(self, length, read=False, priority=0):
        "Increase requirements."
        self.priority = min(self.priority, priority)
        if read:
            self.max_reads += 1
            self.max_read_length = max(self.max_read_length, length)
        else:
            self.max_writes += 1
            self.max_write_length = max(self.max_write_length, length)


    def add_read_request(self, length):
        self.add_request(length, True)

    def add_write_request(self, length):
        self.add_request(length, False)

    def convert_reads_to_writes(self):
        self.max_writes = self.max_requests
        self.max_write_length = self.max_length
        self.max_reads = 0
        self.max_read_length = 0


class ResourceRequirements(dict):
    def __missing__(self, key):
        self[key] = ResourceRequirement(key, 0, 0, 0, 0)
        return self[key]


def initialize_resource_model(taskset):
    for t in taskset:
        # mapping of res_id to ResourceRequirement object
        t.resmodel   = ResourceRequirements()

# Below: alternate resource model that can represent nested critical sections.

class CriticalSection(object):
    def __init__(self, res_id, cs_length):
        self.res_id = res_id
        # The critical section length is assumed to NOT include the length of any
        # nested requests.
        self.length = cs_length
        self.nested = []
        self.outer = None

    def __str__(self):
        return "(R%d for %d%s)" % (self.res_id, self.length,
            "" if not self.nested else
            (", [%s]" % (";".join([str(cs) for cs in self.nested]))))

    @property
    def total_length(self):
        return sum((cs.total_length for cs in self.nested), self.length)

    def add_nested(self, cs, allow_reentrant=False):
        cs.outer = self
        if not allow_reentrant:
            assert not cs.is_reentrant()
        self.nested.append(cs)

    def is_reentrant(self):
        "check if this lock is already being held"
        outer = self.outer
        while outer:
            if outer.res_id == self.res_id:
                return True
            outer = outer.outer
        return False

    def is_nesting_well_ordered(self):
        """check that nesting is consistent with order reflected by increasing
           resource IDs"""
        for cs in self.nested:
            if cs.res_id <= self.res_id:
                return False
        return True

    def __iter__(self):
        return chain([self], *self.nested)

class OutermostCriticalSections(list):
    """Alternate resource model that can represent nested critical sections."""

    def add_outermost(self, res_id, cs_length):
        cs = CriticalSection(res_id, cs_length)
        self.append(cs)
        return cs

    def add_nested(self, outer, res_id, cs_length):
        cs = CriticalSection(res_id, cs_length)
        outer.add_nested(cs)
        return cs

    def all_nesting_well_ordered(self):
        "check that any nesting is well-ordered"
        return all( (cs.is_nesting_well_ordered() for cs in self.all()) )

    def all(self):
        "iterator, including nested critical sections"
        return chain(*self)

    def outer(self):
        "iterator, only outermost critical sections"
        return self

    def get_group_lock_model(self, resource_groups):
        model = ResourceRequirements()

        for cs in self:
            group = resource_groups[cs.res_id]
            group_id = min(group)
            model[group_id].add_request(cs.total_length)

        return model

    def all_flat(self):
        """Iterator over sequence of tuples representing all critical sections.
           The format is: (resource id, CS length, outer),
           where outer is either -1 or the _index_ of the outer CS in the sequence."""
        for i, cs in enumerate(self.all()):
            cs.flat_index = i
            yield (cs.res_id, cs.length, cs.outer.flat_index if cs.outer else -1)

    def __str__(self):
        return "[%s]" % (";".join([str(cs) for cs in self]))


def initialize_nested_resource_model(taskset):
    for t in taskset:
        t.critical_sections = OutermostCriticalSections()

def identify_group_locks(taskset):
    groups = {}
    for t in taskset:
        for cs in t.critical_sections.all():
            if not cs.res_id in groups:
                # new resource, haven't seen it yet
                # initially, each resource is its own group
                groups[cs.res_id] = set([cs.res_id])

            if not cs.outer is None:
                # is a nested request
                g1 = groups[cs.outer.res_id]
                g2 = groups[cs.res_id]
                if g1 != g2:
                    # need to merge resource groups
                    g1.update(g2)
                    for res_id in g2:
                        groups[res_id] = g1
    return groups

def convert_to_group_locks(taskset):
    groups = identify_group_locks(taskset)
    for t in taskset:
        t.resmodel = t.critical_sections.get_group_lock_model(groups)
