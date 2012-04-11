%module sim
%{
#define SWIG_FILE_WITH_INIT
#include "tasks.h"
#include "edf/sim.h"
%}

%ignore Task::get_utilization(fractional_t &util) const;
%ignore Task::get_density(fractional_t &density) const;
%ignore Task::bound_demand(const integral_t &time, integral_t &demand) const;
%ignore Task::bound_load const;
%ignore Task::approx_demand const;

%ignore TaskSet::operator[](int);
%ignore TaskSet::operator[](int) const;
%ignore TaskSet::get_utilization const;
%ignore TaskSet::get_density const;
%ignore TaskSet::get_max_density const;
%ignore TaskSet::approx_load const;

#include "tasks.h"
#include "edf/sim.h"
