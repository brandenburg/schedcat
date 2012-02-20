from collections import defaultdict

from schedcat.model.tasks import TaskSystem

def find_connected_components(taskset):
    """Determine sets of tasks that do not share any resources."""
    by_res = defaultdict(set)
    by_task = {}
    for t in taskset:
        accessed = [res_id for res_id in t.resmodel
                    if t.resmodel[res_id].max_requests > 0]
        if accessed:
            res_id = accessed[0]
            by_res[res_id].add(t)
            # merge all others, if they are different
            for other in accessed[1:]:
                by_res[res_id].update(by_res[other])
                by_res[other] = by_res[res_id]
        else:
            # independent task -> singleton set
            by_task[t] = set([t])

    for c in by_res.values():
        for t in c:
            if t in by_task:
                break
            by_task[t] = c

    return by_task, by_res

def find_independent_tasksubsets(taskset):
    by_task, by_res = find_connected_components(taskset)
    done = set()
    subsets = []

    for t in by_task:
        if not t in done:
            subsets.append(TaskSystem(by_task[t]))
            done.update(by_task[t])

    return subsets
