%module locking
%{
#define SWIG_FILE_WITH_INIT
#include "sharedres.h"
%}

%newobject task_fair_mutex_bounds;
%newobject task_fair_rw_bounds;
%newobject phase_fair_rw_bounds;

%newobject global_omlp_bounds;
%newobject global_fmlp_bounds;
%newobject part_omlp_bounds;
%newobject clustered_omlp_bounds;
%newobject clustered_rw_omlp_bounds;

%newobject part_fmlp_bounds;
%newobject mpcp_bounds;
%newobject dpcp_bounds;

%ignore Interference;
%ignore RequestBound;
%ignore TaskInfo;

%ignore ResourceSharingInfo::get_tasks;

%ignore BlockingBounds::raise_request_span;
%ignore BlockingBounds::get_max_request_span;
%ignore BlockingBounds::operator[](unsigned int);
%ignore BlockingBounds::operator[](unsigned int) const;

%ignore ResourceLocality::operator[](unsigned int) const;
%ignore ReplicaInfo::operator[](unsigned int) const;

#include "sharedres.h"

