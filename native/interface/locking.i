%module locking
%{
#define SWIG_FILE_WITH_INIT
#include "sharedres.h"
%}

%newobject task_fair_mutex_bounds;
%newobject task_fair_rw_bounds;
%newobject phase_fair_rw_bounds;
%newobject msrp_bounds_holistic;

%newobject global_omlp_bounds;
%newobject global_fmlp_bounds;
%newobject part_omlp_bounds;
%newobject clustered_omlp_bounds;
%newobject clustered_rw_omlp_bounds;

%newobject part_fmlp_bounds;
%newobject mpcp_bounds;
%newobject dpcp_bounds;
%newobject msrp_bounds;

%newobject global_pip_bounds;
%newobject ppcp_bounds;

%include "sharedres_types.i"

#include "sharedres.h"

