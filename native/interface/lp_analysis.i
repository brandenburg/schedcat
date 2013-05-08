%module lp_analysis
%{
#define SWIG_FILE_WITH_INIT
#include "lp_analysis.h"
%}

%newobject lp_dpcp_bounds;
%newobject lp_dflp_bounds;

%newobject lp_msrp_bounds;
%newobject lp_preemptive_fifo_bounds;
%newobject lp_unordered_bounds;
%newobject lp_prio_bounds;
%newobject lp_prio_fifo_bounds;

%newobject lp_baseline_bounds;

%include "sharedres_types.i"

%include "lp_analysis.h"
