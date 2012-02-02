

class ResourceRequirement(object):
    def __init__(self, res_id, num_writes=1, write_length=1,
                 num_reads=0, read_length=0):
        self.res_id           = res_id
        self.max_writes       = num_writes
        self.max_reads        = num_reads
        self.max_write_length = write_length
        self.max_read_length  = read_length

    @property
    def max_requests(self):
        "Number of requests of any kind."
        return self.max_writes + self.max_reads

    @property
    def max_length(self):
        "Maximum request length (of any kind)."
        return max(self.max_write_length, self.max_read_length)

    def add_request(self, length, read=False):
        "Increase requirements."
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
