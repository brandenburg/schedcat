from __future__ import division
from __future__ import absolute_import

from math import ceil, floor

# various time-related helpers

def us2ms(us):
    return us / 1000

def ms2us(ms):
    return ms * 1000

def sec2us(sec):
    return sec * 1000000

def ms2us_ru(ms):
    "Convert and round up."
    return int(ceil(ms * 1000))

def ms2us_rd(ms):
    "Convert and round down."
    return int(floor(ms * 1000))
