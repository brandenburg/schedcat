#Necessary includes and stuff

from example.generator import generate_taskset_files, \
                              generate_lock_taskset_files
from example.mapping import partition_tasks
from example.overheads import get_oh_object, bound_cfl_with_oh
from example.locking import bound_cfl_with_locks

from schedcat.model.serialize import load

import os

def get_script_dir():
    return os.path.dirname(os.path.realpath(__file__))

def example_overheads():
    script_dir = get_script_dir()
    return get_oh_object(
        script_dir + "/oh_host=ludwig_scheduler=C-FL-L2-RM_stat=avg.csv",
        script_dir +
        "/oh_host=ludwig_scheduler=C-FL-L2-RM_locks=MX-Q_stat=avg.csv",
        script_dir + "/pmo_host=ludwig_background=load_stat=avg.csv",
        "L2")

def nolock_example(task_files):
    oheads = example_overheads()
    for task_file in task_files:
        ts = load(task_file)
        for task in ts:
            task.wss = 256
        clusts = partition_tasks(2, 12, True, ts)
        if clusts and bound_cfl_with_oh(oheads, True, clusts):
            yield (task_file, clusts)
        else:
            yield (task_file, None)

def lock_example(task_files):
    oheads = example_overheads()
    for task_file in task_files:
        ts = load(task_file)
        for task in ts:
            task.wss = 256
        clusts = partition_tasks(2, 12, True, ts)
        if clusts:
            clusts2 = bound_cfl_with_locks(ts, clusts, oheads, 2)
            if clusts2:
                yield (task_file, clusts2)
            else:
                yield (task_file, None)
        else:
            yield (task_file, None)

def generate_random_nolock_sets():
    return generate_taskset_files("uni-medium", "uni-moderate", 12, 2)

def generate_random_lock_sets():
    return generate_lock_taskset_files("uni-medium", "uni-moderate", 6,
                                       "medium", 6, 0.1, 2)

def print_bounds(results_list):
    for task_file, clusts in results_list:
        print "Processed {}".format(task_file)
        if clusts is not None:
            for clust in clusts:
                for task in clust:
                    print task.response_time - task.deadline
