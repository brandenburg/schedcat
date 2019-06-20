"""Response-time analysis for global fixed-priority scheduling according
to Guan et al., "New Response Time Bounds for Fixed Priority
Multiprocessor Scheduling", published at RTSS'09

Note: this implementation covers only the constrained-deadline case.
"""



from __future__ import division

from math import ceil, floor
from itertools import izip

def is_schedulable(num_cpus, tasks):
    return all(rta_schedulable_guan(k, tasks, num_cpus) for k in xrange(len(tasks)))

bound_response_times = is_schedulable

#
# Task t_k is the task under analysis.
#
def rta_schedulable_guan(k, taskset, num_cpus):
    "Assumes k = index = priority"
    #Use Guan et al.'s response-time analysis
    #in RTSS 2009 (an extension of Baruah's RTSS'07 paper)
    tk = taskset[k]

    assert tk.deadline <= tk.period # only constrained deadlines covered

    if k < num_cpus:
        if tk.cost > tk.deadline:
            return False
        else:
            tk.response_time = tk.cost
            return True

    test_end = tk.deadline
    time = tk.cost
    while time <= test_end:
        #Eq. 12 in Guan's RTSS'09 paper
        interference = total_interference(k, taskset, num_cpus, time)
        interference = int(floor(interference  / num_cpus))

        demand = tk.cost + interference

        if demand == time:
            # demand will be met by time
            tk.response_time = time
            return True
        else:
            # try again
            time = demand

    return False


#carry-in interference of task "task" during an interval of
#length "time", based on Eq. 6 and Eq. 8 in Guan's RTSS'09 paper
def interference_with_carry_in(ti, tk, time):
    #Eq. 6 in Guan's RTSS'09 paper
    #alpha = [[x - Ci]_0 mod Ti - (Ti - Ri)]_0^Ci-1
    alpha_tmp = max(0,  time - ti.cost ) % ti.period - (ti.period - ti.response_time)
    alpha = min(max( alpha_tmp, 0 ), ti.cost - 1)

    #Wk^{CI}(ti, x) = lfloor [x - Ci]_0 / Ti  rfloor * Ci + Ci + alpha
    wk_ci = floor(max(time - ti.cost, 0) / ti.period) * ti.cost + ti.cost + alpha

    #Eq. 8 in Guan's RTSS'09 paper
    #[ Wk^{CI}(ti, x) ]_0^{x - Ck + 1}
    ik_ci = min(max(wk_ci, 0), time - tk.cost + 1)

    return ik_ci

#non-carry-in interference of task "task" during an interval of
#length "time", based on Eq. 5 and Eq. 7 in Guan's RTSS'09 paper
def interference_without_carry_in(ti, tk, time):
    #Eq. 5 in Guan's RTSS'09 paper
    #Wk^{NC}(ti, x) = lfloor x / Ti rfloor * Ci + [x mod Ti]^{Ci}
    wk_nc = floor(time / ti.period) * ti.cost + min(time % ti.period, ti.cost)

    #Eq. 7 in Guan's RTSS'09 paper
    #[ Wk^{NC}(ti, x) ]_0^{x - Ck + 1}
    ik_nc = min(max(wk_nc, 0), time - tk.cost + 1)

    return ik_nc

#The total interference Omega_k(x), in Eq. 9 in Guan's RTSS'09 paper
def total_interference(k, taskset, num_cpus, time):
    tk = taskset[k]
    higher_prio = taskset[:k]

    # compute higher-prio interference without carry-in
    interf_nc = [interference_without_carry_in(ti, tk, time)
                 for ti in higher_prio]

    # the difference of each higher-priority task's carry-in interference and
    # non-carry-in interference
    # calculate the difference of carry-in interference and non-carry-in interference
    # corresponds to Eq. 6 in Baruah's RTSS'07 paper
    idiff = [interference_with_carry_in(ti, tk, time) - inc
             for (ti, inc) in izip(higher_prio, interf_nc)]

    # All HP tasks are partitioned into tau_NC and tau_CI for use
    # in Eq. 9 in Guan's RTSS'09 paper. For tau_CI, we select the m-1
    # tasks with the maximal difference.
    idiff.sort(reverse=True)
    num_carry_in = num_cpus - 1
    omega = sum(idiff[:num_carry_in])

    # Omega_k(x) can be bounded according to Theorem 2 in Baruah's RTSS'07 paper
    # Omega_k(x)= sum_dif + sum_{i<k} ik_nc
    # add the interference for no-carry-in for ALL tasks, tau_CI and tau_NC
    omega += sum(interf_nc)

    # We added the no-carry-in interference for all tasks, and the difference
    # for all tasks in tau_CI. Overall, omega is the sum of
    # no-carry-in-interference for tasks in tau_NC, and
    # carry-in-interference for tasks in tau_CI.
    return omega
