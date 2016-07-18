#ifndef QPA_MSRP_H
#define QPA_MSRP_H

#include "time-types.h"
#include "sharedres.h"
#include "sharedres_types.h"

#include "tasks.h"
#include "schedulability.h"

#include "qpa.h"

class QPA_MSRPTest : public QPATest
{

private:

    unsigned long max_relative_deadline;
    unsigned int num_cpus;
    unsigned int cpu_id;

    const ResourceSharingInfo& info;

 public:

    QPA_MSRPTest(unsigned int num_processors, const ResourceSharingInfo& _info,
                 unsigned int _num_cpus, unsigned int _cpu_id); // Needed by msrp_bounds

    integral_t get_demand(integral_t interval, const TaskSet &ts);
    integral_t get_max_interval(const TaskSet &ts, const fractional_t& util);


    void set_max_relative_deadline(unsigned long d)
        {max_relative_deadline = d;}

};

#endif
