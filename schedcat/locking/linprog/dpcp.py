from collections import defaultdict

from .common import var_direct, var_indirect, var_preempt, add_var_mutex_constraints
from .common import set_blocking_objective, add_topology_constraints, enumerate_requests, find_per_cluster_resources
from .common import find_per_resource_tasks
from schedcat.util.linprog import LinearProgram

def find_prio_ceilings(taskset, resource_mapping):
    per_resource_tasks = find_per_resource_tasks(taskset)
    prio_ceilings = {}
    for res_id in resource_mapping:
        prio_ceilings[res_id] = len(taskset) + 1
    for res_id in resource_mapping:
        for t in per_resource_tasks[res_id]:
            if t.id < prio_ceilings[res_id]:
                prio_ceilings[res_id] = t.id
    return prio_ceilings

def add_independent_cluster_constraints(resource_mapping, tasks, ti, linprog):
    """Constraint 8: no direct or indirect blocking due to resources
    on clusters that ti doesn't even access.
    """

    accessed_clusters = set((resource_mapping[res_id] for res_id in ti.resmodel))

    for tx in tasks.without(ti):
        for res_id_q in tx.resmodel:
            c = resource_mapping[res_id_q]
            if c not in accessed_clusters:
                # cannot block us
                vector = []
                for req_num in enumerate_requests(tx, ti, res_id_q):
                    xd = var_direct(tx.id, res_id_q, req_num)
                    xi = var_indirect(tx.id, res_id_q, req_num)
                    vector += [ (1, xd), (1, xi) ]
                # total contribution == 0
                linprog.add_equality(vector, 0)


def bound_max_waiting_time(resource_mapping, prio_ceilings,
                           taskset, ti, requested_id):
    req_cluster = resource_mapping[requested_id]
    own_length  = ti.resmodel[requested_id].max_length
    assert ti.resmodel[requested_id].max_requests > 0

    # compute maximum lower-priority request length
    delay_by_lower  = 0

    for tx in taskset.with_lower_priority_than(ti):
        for res_id in tx.resmodel:
            ceiling = prio_ceilings[res_id]
            if req_cluster == resource_mapping[res_id] and ceiling <= ti.id:
                # is on the same processor and can block
                length = tx.resmodel[res_id].max_length
                # lower-priority => block once
                delay_by_lower = max(delay_by_lower, length)

    # start with own request length
    wait_time = own_length

    while True:
        delay_by_higher = 0

        for tx in taskset.with_higher_priority_than(ti):
            tx_jobs = tx.maxjobs(wait_time)
            for res_id in tx.resmodel:
                if req_cluster == resource_mapping[res_id]:
                    # is on the same processor
                    length = tx.resmodel[res_id].max_length
                    # higher-priority => block repeatedly
                    num = tx.resmodel[res_id].max_requests
                    delay_by_higher += tx_jobs * num * length

        new_estimate = own_length + delay_by_lower + delay_by_higher
        if new_estimate == wait_time:
            return wait_time
        elif new_estimate > ti.response_time:
            return False
        else:
            wait_time = new_estimate


class LazyWaitBounds(object):
    """Only compute wait bounds on demand, cache results."""
    def __init__(self, *fixed_args):
        self.fixed_args = list(fixed_args)
        self.wait_bounds = {}

    def __getitem__(self, res_id):
        if not res_id in self.wait_bounds:
            args = self.fixed_args + [res_id]
            self.wait_bounds[res_id] = bound_max_waiting_time(*args)
        return self.wait_bounds[res_id]

def add_max_wait_time_constraints(resource_mapping, tasks, ti, linprog,
                                  prio_ceilings=None,
                                  per_cluster_resources=None):
    """Constraint 8: Ti's maximum wait times limit the maximum number of times
    that higher-priority tasks can directly and indirectly delay Ti.
    """
    if prio_ceilings is None:
        prio_ceilings = find_prio_ceilings(tasks, resource_mapping)

    # First compute the maximum wait times for each resource.
    # This is each W_{i,q} in the paper.
    # We do this lazily because we don't need the response times if
    # there are not higher-priority tasks with conflicting requests.
    wait_bounds = LazyWaitBounds(resource_mapping, prio_ceilings, tasks, ti)

    # Second, figure out which resources are on the same processor.
    if per_cluster_resources is None:
        per_cluster_resources = find_per_cluster_resources(resource_mapping, tasks)

    # Apply Lemma 15.
    for tx in tasks.with_higher_priority_than(ti):
        for res_id_q in tx.resmodel:
            # sum up right-hand side
            c = resource_mapping[res_id_q]
            num_per_job = tx.resmodel[res_id_q].max_requests

            bound = 0
            unbounded = False
            # everything accessed by Ti in the same cluster as res_id_q
            for res_id_y in per_cluster_resources[c]:
                if res_id_y in ti.resmodel:
                    max_wait_time = wait_bounds[res_id_y]
                    if max_wait_time is False:
                        # oops, did not converge => can't derive an upper bound
                        unbounded = True
                        break
                    tx_jobs = tx.maxjobs(max_wait_time)
                    ti_reqs = ti.resmodel[res_id_y].max_requests
                    bound += num_per_job * tx_jobs * ti_reqs

            if not unbounded:
                # sum up left-hand side
                vector = []
                for req_num in enumerate_requests(tx, ti, res_id_q):
                    xd = var_direct(tx.id, res_id_q, req_num)
                    xi = var_indirect(tx.id, res_id_q, req_num)
                    vector += [ (1, xd), (1, xi) ]

                # add constraint
                linprog.add_inequality(vector, bound)


def add_conflict_set_constraints(taskset, ti, linprog,
                                 resource_mapping, prio_ceilings=None):
    """Constraint 6: Requests for resources with priority ceilings lower than
    ti's priority cannot delay ti directly or indirectly.
    """
    if prio_ceilings is None:
        prio_ceilings = find_prio_ceilings(taskset, resource_mapping)

    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            ceiling = prio_ceilings[res_id]
            # assumption: lower id == higher priority
            if ceiling > ti.id:
                # lower priority ceiling, cannot block (in)directly
                for req_num in enumerate_requests(tx, ti, res_id):
                    xd = var_direct(tx.id, res_id, req_num)
                    xi = var_indirect(tx.id, res_id, req_num)
                    linprog.equality(1, xd, 1, xi, equal_to=0)

def add_atmostonce_lower_prio_constraints(tasks, ti, linprog, resource_mapping,
                                          prio_ceilings=None, res_per_cluster=None):
    """Constraint 7: Each request can be directly delayed at most once by a
    lower-priority task.
    """
    if res_per_cluster is None:
        res_per_cluster = find_per_cluster_resources(resource_mapping)
    if prio_ceilings is None:
        prio_ceilings = find_prio_ceilings(tasks, resource_mapping)
    clusters = set(resource_mapping.values())

    for c in clusters:
        LHS = []
        for tx in tasks.with_lower_priority_than(ti):
            for res_id in tx.resmodel:
                ceiling = prio_ceilings[res_id]
                # assumption: lower id == higher priority
                if ceiling <= ti.id:
                    #for all v
                    for req_num in enumerate_requests(tx, ti, res_id):
                        LHS.append((1, var_direct(tx.id, res_id, req_num)))
        if LHS:
            res_on_c = res_per_cluster[c]
            req_to_c = sum((ti.resmodel[r].max_requests for r in ti.resmodel
                            if r in res_on_c))
            linprog.add_inequality(LHS, req_to_c)

def get_lp_for_task(resource_locality,
                    taskset, ti, use_rta=True):
    lp = LinearProgram()
    set_blocking_objective(resource_locality, taskset, ti, lp)
    # Constraint 1
    add_var_mutex_constraints(taskset, ti, lp)
    # Constraint 2
    add_topology_constraints(resource_locality, taskset, ti, lp)

    # Constraint 6
    add_conflict_set_constraints(taskset, ti, lp, resource_locality)
    # Constraint 7
    add_atmostonce_lower_prio_constraints(taskset, ti, lp, resource_locality)

    if use_rta:
        # Constraint 8
        add_max_wait_time_constraints(resource_locality, taskset, ti, lp)
    else:
        add_independent_cluster_constraints(resource_locality, taskset, ti, lp)

    return lp
