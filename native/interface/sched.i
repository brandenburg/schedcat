%module sched
%{
#define SWIG_FILE_WITH_INIT
#include "tasks.h"
#include "schedulability.h"
#include "edf/baker.h"
#include "edf/gfb.h"
#include "edf/baruah.h"
#include "edf/bcl.h"
#include "edf/bcl_iterative.h"
#include "edf/rta.h"
#include "edf/ffdbf.h"
#include "edf/load.h"
#include "edf/gedf.h"
#include "edf/gel_pl.h"
#include "edf/qpa.h"
#include "edf/la.h"
%}

%ignore Task::get_utilization(fractional_t &util) const;
%ignore Task::get_density(fractional_t &density) const;
%ignore Task::bound_demand(const integral_t &time, integral_t &demand) const;
%ignore Task::bound_load const;
%ignore Task::approx_demand const;
%ignore Task::dbf;

%ignore TaskSet::operator[](int);
%ignore TaskSet::operator[](int) const;
%ignore TaskSet::get_utilization const;
%ignore TaskSet::get_density const;
%ignore TaskSet::get_max_density const;
%ignore TaskSet::approx_load const;

#include "tasks.h"
#include "schedulability.h"
#include "edf/baker.h"
#include "edf/gfb.h"
#include "edf/baruah.h"
#include "edf/bcl.h"
#include "edf/bcl_iterative.h"
#include "edf/rta.h"
#include "edf/ffdbf.h"
#include "edf/load.h"
#include "edf/gedf.h"
#include "edf/gel_pl.h"
#include "edf/qpa.h"
#include "edf/la.h"
