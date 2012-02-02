from __future__ import division

from bisect import bisect_left as find_index

def is_integral(x):
    return type(x) == int or type(x) == long

def gcd(a,b):
    if a == 0:
        return b
    return abs(gcd(b % a, a))

def lcm(*args):
    if not args:
        return 0
    a = args[0]
    for b in args[1:]:
        if not is_integral(a) or not is_integral(b):
            # only well-defined for integers
            raise Exception, \
                "LCM is only well-defined for integers (got: %s, %s)" \
                % (type(a), type(b))
        a = (a // gcd(a,b)) * b
    return a

def topsum(lst, fun, n):
    """return the sum of the top n items of map(fun, lst)"""
    x = map(fun, lst)
    x.sort(reverse=True)
    return sum(x[0:n])

class LinearEqu(object):
    def __init__(self, a, b):
        self.a = a
        self.b = b

    def __str__(self):
        slope = round(self.b, 3)
        if abs(slope) >= 0.001:
            return '%.3f%+.3f n' % (self.a, slope)
        else:
            return '%.3f' % self.a

    def __call__(self, x):
        return self.a + (self.b * x if self.b else 0)

    def __add__(self, other):
        return LinearEqu(self.a + other.a, self.b + other.b)

    def __mul__(self, scalar):
        return LinearEqu(self.a * scalar, self.b * scalar)

    def __rmul__(self, scalar):
        return self * scalar

    def is_constant(self):
        return self.b == 0

class PieceWiseLinearEqu(object):
    def __init__(self, points):
        # points = [(x1, y1), (x2, y2), ...]
        assert len(points) >= 2

        def slope(i):
            dy = points[i+1][1] - points[i][1]
            dx = points[i+1][0] - points[i][0]
            if dx != 0:
                return dy / dx
            else:
                # De-generate case; the function is not continuous
                # This slope is used in a dummy segment and hence not
                # important.
                return 0.0

        def yintercept(i):
            x, y = points[i]
            dy = slope(i) * x
            return y - dy

        self.segments = [LinearEqu(yintercept(i), slope(i))
                         for i in xrange(len(points) - 1)]
        self.lookup = [points[i+1][0] for i in xrange(len(points) - 1)]
        self.hi     = len(self.lookup) - 1

    def __call__(self, x):
        # find appropriate linear segments
        i = find_index(self.lookup, x, hi=self.hi)
        f = self.segments[i]
        # approximate linearly from support point
        # negative overheads make no sense, so avoid them
        return max(0, f(x))

    def is_constant(self):
        return all([seg[1].is_constant() for seg in self.segments])

def const(x):
    return LinearEqu(x, 0)

def lin(a, b):
    return LinearEqu(a, b)

def scale(alpha, fun):
    return lambda x: fun(x) * alpha

def piece_wise_linear(points):
    return PieceWiseLinearEqu(points)

def make_monotonic(points):
    filtered = points[:1]
    prevprev = None
    _, prev = filtered[0]
    for (x, y) in points[1:]:
        # y values should not decrease
        y = max(prev, y)
        if not prevprev is None and prevprev == y:
            # remove useless intermediate point
            filtered.pop()
        filtered.append((x,y))
        prevprev, prev = prev, y

    # also remove the last one if it is not needed (i.e., constant)
    if len(filtered) == 2 and filtered[-1][1] == filtered[-2][1]:
        filtered.pop()

    return filtered

def is_monotonic(points):
    x1, y1 = points[0]
    for (x2, y2) in points[1:]:
        assert x1 < x2
        if y1 > y2:
            return False
        x1, y1 = x2, y2
    return True

def monotonic_pwlin(points):
    ascending = make_monotonic(points)
    if len(ascending) > 1:
        return piece_wise_linear(ascending)
    elif ascending:
        return const(ascending[0][1])
    else:
        return const(0)
