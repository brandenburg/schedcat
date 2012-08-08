from __future__ import division
from collections import defaultdict

class VariableNameCache(dict):
    def __init__(self, basename):
        self.basename = basename

    def __missing__(self, key):
        # unpack key tuple
        task_id, resource_id, request_num = key
        var = '%s_%d_%d_%d' % (self.basename, task_id, resource_id, request_num)
        self[key] = var
        return var

    def __call__(self, task_id, resource_id, request_num):
        return self[(task_id, resource_id, request_num)]

var_direct   = VariableNameCache('XD')
var_indirect = VariableNameCache('XI')
var_preempt  = VariableNameCache('XP')

def max_num_requests(tx, ti, res_id):
    return tx.maxjobs(ti.response_time) * tx.resmodel[res_id].max_requests

def enumerate_requests(tx, ti, res_id):
    if res_id in tx.resmodel:
        return xrange(max_num_requests(tx, ti, res_id))
    else:
        return []

def find_per_cluster_resources(resource_mapping, taskset=None):
    per_cluster_resources = defaultdict(set)

    if taskset is None:
        # default: count all resources
        for res_id in resource_mapping:
            c = resource_mapping[res_id]
            per_cluster_resources[c].add(res_id)
    else:
        # only count resources accessed by tasks
        for tx in taskset:
            for res_id in tx.resmodel:
                c = resource_mapping[res_id]
                per_cluster_resources[c].add(res_id)

    return per_cluster_resources

def set_blocking_objective(resource_mapping,
                           taskset, ti, linprog):
    objective = []
    local     = []
    remote    = []

    # direct blocking
    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            is_local = resource_mapping[res_id] == ti.partition
            for req_num in enumerate_requests(tx, ti, res_id):
                xd = var_direct(tx.id, res_id, req_num)
                coeff = tx.resmodel[res_id].max_length
                objective.append( (coeff, xd) )
                if is_local:
                    local.append( (coeff, xd) )
                else:
                    remote.append( (coeff, xd) )

    # indirect blocking
    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            is_local = resource_mapping[res_id] == ti.partition
            # compute adjusted indirect delay coefficient
            on_cluster = resource_mapping[res_id]
            coeff = tx.resmodel[res_id].max_length
            for req_num in enumerate_requests(tx, ti, res_id):
                xi = var_indirect(tx.id, res_id, req_num)
                objective.append( (coeff, xi) )
                if is_local:
                    local.append( (coeff, xi) )
                else:
                    remote.append( (coeff, xi) )

    # preemption blocking
    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            is_local = resource_mapping[res_id] == ti.partition
            for req_num in enumerate_requests(tx, ti, res_id):
                xp = var_preempt(tx.id, res_id, req_num)
                coeff = tx.resmodel[res_id].max_length
                objective.append( (coeff, xp) )
                if is_local:
                    local.append( (coeff, xp) )
                else:
                    remote.append( (coeff, xp) )

    linprog.set_objective(objective)
    linprog.local_objective = local
    linprog.remote_objective = remote

def add_topology_constraints(resource_mapping,
                             taskset, ti, linprog):
    """Constraint 2: no remote preemption blocking
    """

    vector = []

    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            if resource_mapping[res_id] != ti.partition:
                for req_num in enumerate_requests(tx, ti, res_id):
                    xp = var_preempt(tx.id, res_id, req_num)
                    vector.append( (1, xp) )

    if vector:
        linprog.add_equality(vector, 0)

def add_var_mutex_constraints(taskset, ti, linprog):
    """Constraint 1: the three kinds of blocking a mutually exclusive
    """
    for tx in taskset.without(ti):
        for res_id in tx.resmodel:
            for req_num in enumerate_requests(tx, ti, res_id):
                xd = var_direct(tx.id, res_id, req_num)
                xi = var_indirect(tx.id, res_id, req_num)
                xp = var_preempt(tx.id, res_id, req_num)
                linprog.inequality(1, xd, 1, xi, 1, xp, at_most=1)

def find_per_resource_tasks(taskset):
    per_resource_tasks = defaultdict(set)
    for tx in taskset:
        for res_id in tx.resmodel:
            per_resource_tasks[res_id].add(tx)
    return per_resource_tasks


def example():
    from schedcat.util.linprog import LinearProgram
    from schedcat.model.tasks import SporadicTask, TaskSystem
    from schedcat.model.resources import initialize_resource_model
    import schedcat.util.linprog

    t1 = SporadicTask(10, 100)
    t2 = SporadicTask(25, 200)
    t3 = SporadicTask(33, 33)
    ts = TaskSystem([t1, t2, t3])

    ts.assign_ids()
    initialize_resource_model(ts)

    t1.resmodel[0].add_request(1)
    t2.resmodel[0].add_request(2)
    t3.resmodel[0].add_request(3)

    for t in ts:
        t.response_time = t.period
        t.partition =  t.id % 2

    # only one resource, assigned to the first processor
    resource_locality = { 0: 0 }

    lp = LinearProgram()
    # Configure blocking objective.
    set_blocking_objective(resource_locality, ts, t1, lp)

    print lp

    print '*' * 80
    print 'Adding mutex constraints:'
    add_var_mutex_constraints(ts, t1, lp)
    print lp

    print '*' * 80
    print 'Adding toplogy constraints:'
    add_topology_constraints(resource_locality, ts, t1, lp)
    print lp

    from .dflp import get_lp_for_task as get_dflp_lp
    from .dpcp import get_lp_for_task as get_dpcp_lp

    print '*' * 80
    print 'DFLP LP:'
    lp = get_dflp_lp(resource_locality, ts, t1)
    print lp

    lp.kill_non_positive_vars()
    print 'DFLP LP (simplified)'
    print lp

    if schedcat.util.linprog.cplex_available:
        sol = lp.solve()

        print 'Solution: local=%d remote=%d' % \
            (sol.evaluate(lp.local_objective), sol.evaluate(lp.remote_objective))
        for x in sol:
            print x, '->', sol[x]

    print '*' * 80
    print 'DPCP LP:'
    lp = get_dpcp_lp(resource_locality, ts, t1)
    print lp

    lp.kill_non_positive_vars()
    print 'DPCP LP (simplified)'
    print lp

    if schedcat.util.linprog.cplex_available:
        sol = lp.solve()

        print 'Solution: local=%d remote=%d' % \
            (sol.evaluate(lp.local_objective), sol.evaluate(lp.remote_objective))
        for x in sol:
            print x, '->', sol[x]


if __name__ == '__main__':
    example()
