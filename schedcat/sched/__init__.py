
# Python model to C++ model conversion code.


try:
    from .native import TaskSet

    using_native = True

    def get_native_taskset(tasks):
        ts = TaskSet()
        for t in tasks:
            if (hasattr(t, 'pp')):
                ts.add_task(t.cost, t.period, t.deadline, t.pp)
            else:
                ts.add_task(t.cost, t.period, t.deadline)
        return ts

except ImportError:
    # Nope, C++ impl. not available. Use Python implementation.
    using_native = False
    def get_native_taskset(tasks):
        assert False # C++ implementation not available
