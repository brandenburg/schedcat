from collections import defaultdict

from .common import var_direct, var_indirect, var_preempt, add_var_mutex_constraints
from .common import set_blocking_objective, add_topology_constraints, enumerate_requests, find_per_cluster_resources
from collections import defaultdict
from schedcat.util.linprog import LinearProgram

def add_fifo_resource_constraints(taskset, ti, linprog):
    """Constraint 4: only one *directly* blocking request each time a job of T_i
    issues a request, with regard to each resource and each task."""
    # Count how many times ti accesses something on each cluster.
    # int() defaults to zero, which is exactly what we want.

    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            vector = []
            for req_num in enumerate_requests(tx, ti, res_id):
                xd = var_direct(tx.id, res_id, req_num)
                vector += [ (1, xd) ]
            # Add the constraint.
            limit = ti.resmodel[res_id].max_requests if res_id in ti.resmodel else 0
            linprog.add_inequality(vector, limit)


def add_fifo_cluster_constraints(resource_mapping,
                                 taskset, ti, linprog):
    """Constraint 5: only one blocking request each time a job of T_i
    issues a request, with regard to each cluster and each task. """
    # Count how many times ti accesses something on each cluster.
    # int() defaults to zero, which is exactly what we want.
    per_cluster_counts = defaultdict(int)

    for res_id in ti.resmodel:
        c = resource_mapping[res_id]
        per_cluster_counts[c] += ti.resmodel[res_id].max_requests

    for tx in taskset.without(ti):
        # We need to add one constraint for each cluster (relevant to Tx).
        per_cluster_constraints = defaultdict(list)

        for res_id in tx.resmodel:
            c = resource_mapping[res_id]
            vector = per_cluster_constraints[c]

            for req_num in enumerate_requests(tx, ti, res_id):
                xd = var_direct(tx.id, res_id, req_num)
                xi = var_indirect(tx.id, res_id, req_num)
                vector += [ (1, xd), (1, xi) ]

        # Add the constraints.
        for c in per_cluster_constraints:
            vector = per_cluster_constraints[c]
            limit  = per_cluster_counts[c]
            linprog.add_inequality(vector, limit)


def get_lp_for_task(resource_locality,
                    taskset, ti):
    lp = LinearProgram()
    set_blocking_objective(resource_locality, taskset, ti, lp)
    # Constraint 1
    add_var_mutex_constraints(taskset, ti, lp)
    # Constraint 2
    add_topology_constraints(resource_locality, taskset, ti, lp)
    # Constraint 4
    add_fifo_resource_constraints(taskset, ti, lp)
    # Constraint 5
    add_fifo_cluster_constraints(resource_locality, taskset, ti, lp)
    return lp
