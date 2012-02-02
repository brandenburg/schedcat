#ifndef EDF_SIM_H
#define EDF_SIM_H

struct Stats
{
    unsigned long num_tardy_jobs;
    unsigned long num_ok_jobs;
    unsigned long total_tardiness;
    unsigned long max_tardiness;
    unsigned long first_miss;
};

bool edf_misses_deadline(unsigned int num_procs,
                         TaskSet &ts,
                         unsigned long end_of_simulation);

unsigned long edf_first_violation(unsigned int num_procs,
                                  TaskSet &ts,
                                  unsigned long end_of_simulation);

Stats edf_observe_tardiness(unsigned int num_procs,
                            TaskSet &ts,
                            unsigned long end_of_simulation);

#endif
