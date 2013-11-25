#Necessary includes and stuff

from schedcat.overheads.model import Overheads, CacheDelay
from schedcat.overheads.jlfp import charge_scheduling_overheads, \
                                    quantize_params
from schedcat.sched.edf.gel_pl import bound_gfl_response_times, \
                                      has_bounded_tardiness

def copy_lock_overheads(oh, lock_oh):
    oh.lock = lock_oh.lock
    oh.unlock = lock_oh.unlock
    oh.read_lock = lock_oh.read_lock
    oh.read_unlock = lock_oh.read_unlock
    oh.syscall_in = lock_oh.syscall_in
    oh.syscall_out = lock_oh.syscall_out

def get_oh_object(basic_oh, lock_oh, cache_oh, cache_level):
    oh = Overheads.from_file(basic_oh)
    oh.initial_cache_loss = \
            CacheDelay.from_file(cache_oh).__dict__[cache_level]
    oh.cache_affinity_loss = \
            CacheDelay.from_file(cache_oh).__dict__[cache_level]
    if lock_oh is not None:
        lock_oh = Overheads.from_file(lock_oh)
        copy_lock_overheads(oh, lock_oh)
    return oh

#Assumes absence of locking
def bound_cfl_with_oh(oheads, dedicated_irq, clusts):
    for clust in clusts:
        success = charge_scheduling_overheads(oheads, clust.cpus,
                                              dedicated_irq,
                                              clust)
        quantize_params(clust)
        if (success and has_bounded_tardiness(clust.cpus, clust)):
            bound_gfl_response_times(clust.cpus, clust, 15)
        else:
            return False
    return True
