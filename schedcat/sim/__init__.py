from .native import TaskSet

def get_native_taskset(tasks):
    ts = TaskSet()
    for t in tasks:
        if t.implicit_deadline():
            ts.add_task(t.cost, t.period)
        else:
            ts.add_task(t.cost, t.period, t.deadline)
    return ts
