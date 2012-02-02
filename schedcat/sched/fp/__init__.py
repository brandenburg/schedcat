"""Fixed-priority schedulability tests.
   Currently, only uniprocessor response-time analysis is implemented.
"""

from __future__ import division

from .rta import bound_response_times, is_schedulable

