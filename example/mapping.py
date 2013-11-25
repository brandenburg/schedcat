#Necessary includes and stuff

from schedcat.mapping.rollback import Bin, WorstFit
from schedcat.model.tasks import SporadicTask, TaskSystem

def partition_tasks(cluster_size, clusters, dedicated_irq,
                    taskset):
    first_cap = cluster_size - 1 if dedicated_irq \
                else cluster_size
    first_bin = Bin(size=SporadicTask.utilization,
                    capacity=first_cap)
    other_bins = [Bin(size=SporadicTask.utilization,
                      capacity=cluster_size)
                  for _ in xrange(1, clusters)]
    heuristic = WorstFit(initial_bins=[first_bin] + other_bins)
    heuristic.binpack(taskset)
    if not (heuristic.misfits):
        clusts = [TaskSystem(b.items) for b in heuristic.bins]
        for i, c in enumerate(clusts):
            if i == 0 and dedicated_irq:
                c.cpus = cluster_size - 1
            else:
                c.cpus = cluster_size
            for task in c:
                task.partition = i
        return [c for c in clusts if len(c) > 0]
    else:
        return False
