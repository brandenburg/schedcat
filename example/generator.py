#Necessary includes and stuff

from schedcat.model.serialize import write
from schedcat.generator.tasksets import mkgen, \
                                        NAMED_UTILIZATIONS, \
                                        NAMED_PERIODS
from schedcat.util.time import ms2us
import schedcat.model.resources as resources
import os
import random

CSLENGTH = { 'short'  : lambda: random.randint(1,   15),
             'medium' : lambda: random.randint(1,  100),
             'long'   : lambda: random.randint(5, 1280), }

def generate_taskset_files(util_name, period_name, cap, number):
    generator = mkgen(NAMED_UTILIZATIONS[util_name],
                      NAMED_PERIODS[period_name])
    generated_sets = []
    for i in range(number):
        taskset = generator(max_util=cap, time_conversion=ms2us)
        filename = "{0}_{1}_{2}_{3}".format(util_name,
                                            period_name, cap, i)
        write(taskset, filename)
        generated_sets.append(filename)
    return generated_sets

def generate_lock_taskset_files(util_name, period_name, cap,
                                cslength, nres, pacc, number):
    generator = mkgen(NAMED_UTILIZATIONS[util_name],
                      NAMED_PERIODS[period_name])
    generated_sets = []
    for i in range(number):
        taskset = generator(max_util=cap, time_conversion=ms2us)
        resources.initialize_resource_model(taskset)
        for task in taskset:
            for res_id in range(nres):
                if random.random() < pacc:
                    nreqs = random.randint(1, 5)
                    length = CSLENGTH[cslength]
                    for j in range(nreqs):
                       task.resmodel[res_id].add_request(length())
        filename = "{0}_{1}_{2}_{3}_{4}_{5}_{6}".format(
                util_name, period_name, cap, cslength, nres, pacc,
                i)
        write(taskset, filename)
        generated_sets.append(filename)
    return generated_sets
