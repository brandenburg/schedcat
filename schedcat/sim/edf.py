
import schedcat.sim as sim
import schedcat.sim.native as cpp

from schedcat.util.time import sec2us


def is_deadline_missed(no_cpus, tasks, simulation_length=60):
    ts = sim.get_native_taskset(tasks)
    return cpp.edf_misses_deadline(no_cpus, ts, int(sec2us(simulation_length)))

def time_of_first_miss(no_cpus, tasks, simulation_length=60):
    ts = sim.get_native_taskset(tasks)
    return cpp.edf_first_violation(no_cpus, ts, int(sec2us(simulation_length)))

def no_counter_example(*args, **kargs):
    return not is_deadline_missed(*args, **kargs)
