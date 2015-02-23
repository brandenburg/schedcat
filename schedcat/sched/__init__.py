
# Python model to C++ model conversion code.


try:
    from .native import TaskSet

    using_native = True

    def get_native_taskset(tasks, with_suspensions=False):
        ts = TaskSet()
        if with_suspensions:
            for t in tasks:
                if (hasattr(t, 'prio_pt')):
                    ts.add_task(t.cost, t.period, t.deadline, t.prio_pt, t.suspended)
                else:
                    ts.add_task(t.cost, t.period, t.deadline, 0, t.suspended)
        else:
            for t in tasks:
                if (hasattr(t, 'prio_pt')):
                    ts.add_task(t.cost, t.period, t.deadline, t.prio_pt)
                else:
                    ts.add_task(t.cost, t.period, t.deadline)
        return ts

except ImportError:
    # Nope, C++ impl. not available. Use Python implementation.
    using_native = False
    using_linprog = False
    def get_native_taskset(tasks):
        assert False # C++ implementation not available

if using_native:
    try:
        from .native import AffinityRestrictions

        using_linprog = True

        def get_native_affinities(tasks):
            afs = AffinityRestrictions()

            for i, t in enumerate(tasks):
                for cpu in t.affinity:
                    afs.add_cpu(i, cpu)

            return afs

    except ImportError:
        using_linprog = False
