import mpmath

from schedcat.model.canbus import CANMessage
from schedcat.model.canbus import CANMessageSet

"""
Implementation of Broster et al.'s probabilistic response time analysis 

    "Probabilistic analysis of CAN with faults"
    I. Bertogna, A. Burns, and G. Rodriguez-Navas,
    Proceedings of the 23rd IEEE International Real-Time Systems Symposium,
    pages 269-278, 2002 

    "Timing analysis of real-time communication under electromagnetic
     interference"
    I. Bertogna, A. Burns, and G. Rodriguez-Navas,
    Real-Time Systems 30.1-2 (2005): 55-81

"""

def get_prob_poisson(events, length, rate):
    """ P(k, lambda = t * rate) = """
    avg_events = mpmath.fmul(rate, length) # lambda
    prob = mpmath.fmul((-1), avg_events)
    for i in range(1, events + 1):
        prob = mpmath.fadd(prob, mpmath.log(mpmath.fdiv(avg_events, i)))
    prob = mpmath.exp(prob)
    return prob

def get_prob_schedulable(msgs, m, maxfaults = None):
    """Returns the probability that a message is schedulable even in the 
    presence of transmission faults. If faults != None, then the probability
    that the message is schedulable assuming #retransmissions=maxfaults is 
    returned. Else, then the cumulative probability is returned. The following 
    iterative equation is used to derive the probability:
    P(R_i_n) = P(n, R_i_n) - 
                \sum_{j=0}^{n-1} [P(R_i_j) * P(n - j, R_i_n - R_i_j)],
    where R_i_n is the worst case response time of message i assuming it is 
    delayed by n retransmissions, P(R_i_n) is the upper bound on probability 
    that message i is affected by exactly n faults, and P(n, R_i_n) is the 
    probability that there are n faults in an interval of length R_i_n 
    (we may use Poisson distribution for this). Thus, P(R_i_0) = P(0, R_i_0).
    """

    faults = 0
    if maxfaults != None:
        if len(m.prob_vec) > maxfaults + 1:
            return m.prob_vec[maxfaults]
        faults = len(m.prob_vec)
    
    while maxfaults == None or faults <= maxfaults:

        wctt = msgs.get_wctt_fast(m, faults)
        wcrt = wctt + m.jitter

        if wcrt > m.deadline:
            break

        prob = get_prob_poisson(faults, wctt, msgs.mfr)
        error = mpmath.mpf(0)

        # DO NOT change the index to 'faults + 1'
        for i in range(0, faults):
            wctt_tmp = msgs.get_wctt_fast(m, i)
            error = mpmath.fadd(error, mpmath.fmul(m.prob_vec[i], \
                        get_prob_poisson(faults - i, wctt - wctt_tmp, msgs.mfr)))

        prob = mpmath.fsub(prob, error)
        assert prob <= 1
        m.prob_vec.append(prob)
        faults += 1

    retval = mpmath.mpf(0)

    # if maxfaults != None and len(prob_vec) < maxfaults + 1:
    #   retval = 0
    
    if maxfaults != None and len(m.prob_vec) >= maxfaults + 1:
            retval = m.prob_vec[maxfaults]

    elif maxfaults == None:
        for prob in m.prob_vec:
            if (mpmath.fadd(retval, prob) <= 1):
                retval = mpmath.fadd(retval, prob)
            else:
                break
   
    assert (retval <= 1)

    if retval > 1:
        retval = mpmath.mpf('1')
    return min(retval, 1)

def is_schedulable(msgs, m, faults):
    if get_prob_schedulable(msgs, m, faults) == 0:
        return False
    else:
        return True
