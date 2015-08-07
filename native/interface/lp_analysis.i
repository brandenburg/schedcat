%module lp_analysis
%{
#define SWIG_FILE_WITH_INIT
#include "lp_analysis.h"
%}

%newobject lp_dpcp_bounds;
%newobject lp_dflp_bounds;

%newobject lp_msrp_bounds;
%newobject lp_pfp_preemptive_fifo_spinlock_bounds;

%newobject lp_pfp_unordered_spinlock_bounds;

%newobject lp_pfp_prio_spinlock_bounds;

%newobject lp_pfp_prio_fifo_spinlock_bounds;

%newobject lp_pfp_baseline_spinlock_bounds;

%newobject lp_global_pip_bounds;
%newobject lp_ppcp_bounds;
%newobject lp_global_fmlpp_bounds;
%newobject lp_sa_gfmlp_bounds;
%newobject lp_prsb_bounds;
%newobject lb_no_progress_fifo_bounds;
%newobject lb_no_progress_priority_bounds;

%newobject dummy_bounds;

%include "sharedres_types.i"

%include "lp_analysis.h"
