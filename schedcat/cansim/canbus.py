import schedcat.cansim as sim
import schedcat.cansim.native as cpp
from schedcat.cansim.native import CANTaskSet

def get_native_canbus_msgset(msgs):
    ts = CANTaskSet()
    for msg in msgs:
        assert msg.implicit_deadline()
        ts.add_canbus_task(msg.max_framesize, msg.period * msgs.busrate, \
                           msg.id, msg.tid)
    ts.set_busrate(msgs.busrate)
    ts.add_fault_params(msgs.po, msgs.mfr)
    ts.mark_critical_tasks(msgs[0].tid) # assume ts[0] is replicated
    ts.set_rprime(msgs.rprime)
    return ts

def completion_time(msgs, sim_len_ms, taskid, priority, seqno):
    sim_len_bit_time = sim_len_ms * msgs.busrate
    ts = get_native_canbus_msgset(msgs)
    return cpp.get_job_completion_time(ts, sim_len_bit_time, taskid, priority, seqno)

def observe_tardiness(msgs, sim_len_ms, boot_time_ms, iterations):
    ts = get_native_canbus_msgset(msgs)
    cpp.simulate_for_tardiness_stats(ts, sim_len_ms, boot_time_ms, iterations)
