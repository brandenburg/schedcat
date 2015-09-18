import math
import copy

from schedcat.model.tasks import SporadicTask
from schedcat.model.tasks import TaskSystem

class CANMessage(SporadicTask):
    def __init__(self, length, period, deadline=None, id=None, tid=None):
        """The structure of a CANMessage is almost similar to SporadicTask, 
        except that a CAN message does not have a WCET (cost), but rather the 
        payload size. We use the same field 'cost' o store the payload size.
        CANMessage also has a 'tid' field. The 'id' is unique per message, but 
        the 'tid' field remains same for message replicas.
        """
        SporadicTask.__init__(self, length, period, deadline, id)
        self.tid = tid
        self.transfer_delay = None
        self.jitter = 0

        self.max_framesize = 44 + (8 * length) 
        self.max_framesize += math.floor((33 + (8 * length)) / 4.0)
        self.max_framesize = int(self.max_framesize)
        self.critical = False

    def utilization(self, tau):
        return (float(self.max_framesize) / self.period) * tau
	 
    def __repr__(self):
        framesizestr = ", framesize=%d" % self.max_framesize
        jitterstr = ", jitter=%d" % self.jitter
        idstr = ", id=%s" % self.id if self.id is not None else ""
        tidstr = ", tid=%s" % self.tid if self.tid is not None else ""
        dstr  = ", deadline=%s" % self.deadline
        return "CANMessage(b=%s, period=%s%s%s%s%s%s)" % (self.cost, \
            self.period, dstr, idstr, tidstr, jitterstr, framesizestr)


class CANMessageSet(TaskSystem):
    def __init__(self, tasks = []):
        self.extend(tasks)
        self.reset()

    def reset(self):
        """Call this function whenever the message set is modified,
        say after replicating messages.
        """
        for m in self:
            m.prob_vec = []
            m.wctt_vec = []
            m.transfer_delay = None
            m.blocking_delay = None
            m.retran_delay_per_fault = None			

    def __str__(self):
        retval = "CANMessageSet (utlization = %f percent)\n" % self.utilization()
        retval += "\n".join([str(t) for t in self]) 
        return retval

    def __repr__(self):
        retval = "CANMessageSet([" + ", ".join([repr(t) for t in self]) + "])\n"
        retval += "Utilization = " + str(self.utilization() * 100) + "%"
        return retval

    def utilization(self):
        util = 0
        for m in self:
            util += float(m.max_framesize) / m.period
        return util * self.tau * 100

    def get_transfer_delay(self, mi):
        """C_i = (44 + 8b + floor((34 + 8b - 1)/4)) * tau,
        where b is the length of payload of message mi (in bytes) and 
        tau is time to transfer one bit (tau-in-ms = 1/busrate-in-kbps)
        """
        if mi.transfer_delay != None:
            return mi.transfer_delay
        mi.transfer_delay = mi.max_framesize * self.tau
        return mi.transfer_delay

    def get_blocking_delay(self, mi):
        """B_i = [max_{mk \in lp(mi)}(C_k)] + S, where
        S is the interframe space
        """
        if mi.blocking_delay != None:
            return mi.blocking_delay
        delay = 0
        for mk in self:
            if mi != mk and mk.id > mi.id: # mk has lower priority
                delay = max(delay, self.get_transfer_delay(mk))
        mi.blocking_delay = delay + self.inter_frame_space
        return mi.blocking_delay

    def get_retran_delay_per_fault(self, mi):
        """E_i(#faults=1) = [max_{mk \in hep(mi)}(C_k)] + E, where
        E is the max error frame size
        """
        if mi.retran_delay_per_fault != None:
            return mi.retran_delay_per_fault
        delay = 0
        for mk in self:
            if mk.id <= mi.id: # mk has higher or equal priority
                delay = max(delay, self.get_transfer_delay(mk))
        mi.retran_delay_per_fault = delay + self.max_error_frame_size
        return mi.retran_delay_per_fault

    def get_inter_delay(self, mi, t):
        """Ii(t) = sum_{mk \in hp(mi)} [ceil((t - Ci + Jk + tau)/Tk)] * (Ck + S)
        """
        delay = 0
        for mk in self:
            if mk.id < mi.id: # mk has higherr priority
                nr = t - self.get_transfer_delay(mi) + mk.jitter + self.tau
                dr = mk.period
                mul = self.get_transfer_delay(mk) + self.inter_frame_space
                delay += math.ceil(float(nr) / dr) * mul
        return delay	

    def get_wctt(self, mi, faults):
        """Returns the worst-case transmission time, i.e., ignoring jitter,
        of the message assuming delays due to #retransmissions=faults, using the
        following recursive equation:
        wctt_i = B_i + C_i + I_i(wctt_i) + E_i(faults), where
        B_i = max blocking delay
        C_i = max transfer delay
        I_i(t) = mac interference delay in a period of length t 
        E_i = max delay due to #faults retransmissions
        """
        transfer_delay = self.get_transfer_delay(mi)
        wctt = wctt_old = transfer_delay

        blocking_delay = self.get_blocking_delay(mi)
        retran_delay_per_fault = self.get_retran_delay_per_fault(mi)
        retran_delay = faults * retran_delay_per_fault
        
        while True:
            inter_delay = self.get_inter_delay(mi, wctt_old)
            wctt_new = blocking_delay + transfer_delay + \
                inter_delay + retran_delay

            if wctt_new == wctt_old:
                return wctt_new

            if wctt_new + mi.jitter > mi.deadline:
                return wctt_new # TODO or return None

            wctt_old = wctt_new

    def get_wctt_fast(self, mi, faults):
        if len(mi.wctt_vec) > faults:
            return mi.wctt_vec[faults]
        startindex = len(mi.wctt_vec)
        for i in range(startindex, faults + 1):
            mi.wctt_vec.append(self.get_wctt(mi, i))
        return mi.wctt_vec[faults]

    def get_max_wcrt(self):
        if hasattr(self, 'max_wcrt') and self.max_wcrt != None:
            return self.max_wcrt
        self.max_wcrt = 0
        for m in self:
            wcrt = self.get_wctt_fast(m, 0) + m.jitter
            if wcrt > self.max_wcrt:
                self.max_wcrt = wcrt
        assert self.max_wcrt != 0 and self.max_wcrt != None
        return self.max_wcrt

    def add_replicas(self, mi, r):
        assert mi.tid != None
        mi.critical = True
        for i in range(0, r):
            mk = copy.deepcopy(mi)
            mk.id = None
            self.append(mk)
        self.reset() # since message set is changed

    def get_replication_factor(self, mi):
        replicas = 0
        for mk in self:
            if mk.tid == mi.tid:
                replicas += 1
        return replicas
