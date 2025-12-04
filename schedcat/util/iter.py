# assorted sequence helpers

from heapq        import heapify, heappop, heappush

class PrioObj(object):
    def __init__(self, val, le):
        self.val = val
        self.le  = le

    def __str__(self):
        return str(self.val)

    def __le__(self, other):
        return self.le(self.val, other.val)

    def __lt__(self, other):
        return self.le(self.val, other.val) and not self.le(other.val, self.val)


def imerge(le, *iters):
    nxtheap = []
    _le = lambda a, b: le(a[0], b[0])
    for i in iters:
        try:
            it = iter(i)
            nxtheap.append(PrioObj((next(it), it), _le))
        except StopIteration:
            pass
    heapify(nxtheap)
    while nxtheap:
        wrapper = heappop(nxtheap)
        x, it = wrapper.val
        yield x
        try:
            wrapper.val = (next(it), it)
            heappush(nxtheap, wrapper)
        except StopIteration:
            pass

def uniq(seq):
    it = iter(seq)
    try:
        last = next(it)
    except StopIteration:
        return
    yield last
    for x in it:
        if x != last:
            last = x
            yield x
