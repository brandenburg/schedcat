import mpmath

from schedcat.model.canbus import CANMessage
from schedcat.model.canbus import CANMessageSet
import schedcat.sched.canbus.broster as br

""" Refer to our following paper for details:
Arpan Gujarati, and Bjoern B. Brandenburg, "When is CAN the Weakest Link? A
Bound on Failures-In-Time in CAN-Based Real-Time Systems", Proceedings of the
36th IEEE Real-Time Systems Symposium (RTSS 2015), to appear, December 2015.
"""

def get_prob_correct_sync(k, pc):
    """prob = \sum_{l=0}^{ceil(k/2)-1} (k_choose_l) * (pc)^l * (1 - pc)^(k-l)
    """
    retval = 0
    lmax = mpmath.ceil(float(k)/2) - 1
    for l in range(0, lmax + 1):
        retval += mpmath.binomial(k,l) * mpmath.power(pc, l) * \
            mpmath.power(1 - pc, k - l)
    return retval

def get_prob_correct_async(k, pc, rprime):
    """prob = \sum_{l=0}^{k-rprime} (k_choose_l) * (pc)^l * (1 - pc)^(k-l)
    """
    retval = 0
    lmax = k - rprime
    for l in range(0, lmax + 1):
        retval += mpmath.binomial(k,l) * mpmath.power(pc, l) * \
            mpmath.power(1 - pc, k - l)
    return retval

def get_prob_time_periodic(msgs, mi, k):
    """ Our timing analysis.
    """
    faults = 0
    prob = 0

    while True:

        timely_replicas = 0
        for mk in msgs:
            if mk.tid == mi.tid and mk.omitted == False:
                if msgs.get_wctt(mk, faults) + mk.jitter <= mk.deadline:
                    timely_replicas += 1

        if timely_replicas < k:
            break

        if timely_replicas == k: 
            prob += br.get_prob_poisson(faults, mi.deadline, msgs.mfr)

        faults += 1

    return prob 

def powerset(ids):
    """Implements an iterator for returning all subsets of set 'ids'.
    We do not return empty set.
    """
    if len(ids) == 1:
        yield ids
        yield []
    else:
        for item in powerset(ids[1:]):
            yield item
            yield [ids[0]] + item
    
def get_prob_schedulable(msgs, mi, boot_time, sync = True):
    """Returns the probability that message m is successfully transmitted even
    in the presence of omission, commission, transmission faults. Here, although
    m represents a single message, we compute the probability of successful
    transmission collectively for all replicas of m, identified by a common
    'tid' field. """
    r = msgs.get_replication_factor(mi)
    rprime = int(mpmath.floor(r / 2.0) + 1)

    prob_msg_corrupted = 1 - br.get_prob_poisson(0, mi.deadline, msgs.po)
    prob_msg_omitted = 1 - br.get_prob_poisson(0, boot_time, msgs.po)

    # since pr(correct) is just a function of k <= r and pc,
    # we compute it beforehand for all values (pc is commission fault prob.)
    prob_correct = []
    for k in range(0, r + 1):
        if sync:
            prob_correct.append(get_prob_correct_sync(k, prob_msg_corrupted))
        else:
            prob_correct.append(get_prob_correct_async(k, prob_msg_corrupted, rprime))

    # need to iterate over all subsets of replica set of m
    replica_ids = []
    for mk in msgs:
        if mk.tid == mi.tid:
            replica_ids.append(mk.id)
    assert r == len(replica_ids)

    prob_success = 0
    for omitted_ids in powerset(replica_ids):
        for mk in msgs:
            if mk.id in omitted_ids:
                mk.omitted = True
            else:
                mk.omitted = False

        s = r - len(omitted_ids)
        prob_omitted = mpmath.power(prob_msg_omitted, r - s) * \
            mpmath.power(1 - prob_msg_omitted, s)

        prob_time_correct = 0
        for k in range(1, s + 1):
            prob_time_correct += get_prob_time_periodic(msgs, mi, k) * \
                                 prob_correct[k]

        prob_success += prob_omitted * prob_time_correct

    return min(prob_success, 1)

def get_fit_rate(mi, prob_success):
    if (prob_success == 1):
        return 0

    lna = mpmath.log(prob_success)
    lnasq = mpmath.fmul(lna, lna)

    term1 = mpmath.fdiv((mpmath.fsub(1, prob_success)), prob_success)
    term2 = mpmath.fdiv(term1, lnasq)

    mabf = term2
    mtbf = mpmath.fdiv(mpmath.fmul(mabf, mi.period), 3600000)

    fitrate = mpmath.fdiv(1000000000, mtbf)
    return fitrate
