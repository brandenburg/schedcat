-include ../config.mk # preferred Makefile config
-include .config	  # include dot file for legacy support

# #### Tools and Paths ####

# default: swig binary available in $PATH.
SWIG     ?= swig
# default: standard python in $PATH.
PYTHON   ?= python2.7
# default: assume GMP is installed in standard location.
GMP_PATH ?= /usr

# #### Flags ####

DISABLED_WARNINGS  = -Wno-unused-parameter # Too much noise.

INCLUDES = -Iinclude -I${GMP_PATH}/include
# on Cygwin it is required to have -lpython2.7
# and it cannot be before the -L on its path
LIBS     = -L${GMP_PATH}/lib -lgmp -lgmpxx
DEFS    ?=

OS := $(shell uname)

# How to link Python libraries?
PYTHON_VER := ${shell ${PYTHON} --version 2>&1}
# Default:  assume Python is in $PATH and can tell us itself where to find the headers.
PYTHON_HEADER_DIR ?= ${shell ${PYTHON} -c "import distutils.sysconfig; print distutils.sysconfig.get_python_inc()" }
PYTHON_INC ?= -I${PYTHON_HEADER_DIR}

ifeq ($(OS),Darwin)
# Mac OS X: link against Python Framework
PYTHON_LIB ?= -framework Python
SOFLAGS     = -dynamiclib
PIC_FLAG    = -fPIC
else ifneq (,$(findstring CYGWIN,$(OS)))
# Cygwin
PYTHON_LIB ?= -lpython2.7
SOFLAGS     = -shared
PIC_FLAG    =
else
# Default/Linux: No special linking required.
PYTHON_LIB ?=
SOFLAGS     = -shared
PIC_FLAG    = -fPIC
endif
${info Linking with ${PYTHON_VER} (headers: ${PYTHON_INC}).}


# Support for clock_gettime()
ifeq ($(OS),Linux)
LIBS += -lrt
endif

# #### CPLEX Support ####

# See if we can find a CPLEX installation.
CPLEX := ${realpath ${shell which cplex}}
ifneq ($(CPLEX),)
# Yes, we seem to have something that looks like cplex.

# The CPLEX variable now contains the canonicalized path to the cplex
# binary. From this, we can extract the include and library paths.
CPLEX_DIR := ${realpath ${dir $(CPLEX)}/../..}
# For at least the CPLEX studio 12.7.1 distribution, "concert" resides outside
# the cplex root directory
CONCERT_DIR := ${realpath ${dir $(CPLEX)}/../../../concert}
CPLEX_PATH ?= $(CPLEX_DIR)
CONCERT_PATH ?= $(CONCERT_DIR)
endif

ifneq ($(CPLEX_PATH),)
${info Linking with CPLEX installation in $(CPLEX_DIR).}
ifneq ($(CONCERT_PATH),)
${info Linking with CPLEX concert installation in $(CONCERT_DIR).}
LIBS     += -L$(CONCERT_PATH)/lib/x86-64_linux/static_pic/
endif
# Add headers, libraries, and definitions.
INCLUDES += -I$(CPLEX_PATH)/include/
LIBS     += -L$(CPLEX_PATH)/lib/x86-64_linux/static_pic/ -lilocplex -lcplex -lconcert -lpthread
DEFS     += -DCONFIG_HAVE_CPLEX -DCONFIG_HAVE_LP
endif


# #### GLPK Support ####

# first try system libraries
GLPK := ${realpath ${shell find /usr/include/ /usr/local/include/ -iname 'glpk.h' 2>/dev/null}}
ifeq ($(GLPK),)
# ok, try finding the frontend binary
GLPK := ${realpath ${shell which glpsol}}
endif

ifneq ($(GLPK),)
GLPK_DIR :=${realpath ${dir $(GLPK)}/..}
GLPK_PATH ?= $(GLPK_DIR)
endif

ifneq ($(GLPK_PATH),)
${info Linking with GLPK installed at prefix=$(GLPK_DIR).}
# wildcard avoid to include non-existent directories (cygwin)
INCLUDES += -I$(wildcard $(GLPK_PATH)/include)
LIBS     += -L$(wildcard $(GLPK_PATH)/lib) -lglpk
DEFS     += -DCONFIG_HAVE_GLPK -DCONFIG_HAVE_LP
endif

SWIG_DEFS := ${DEFS}

# #### Debug support ####
ifeq ($(DEBUG),y)
DEFS += -g -DDEBUG -march=native -Werror
else
DEFS += -O2 -march=native
endif

# disable assertions only if explicitly requested
ifeq ($(DEBUG),n)
DEFS += -DNDEBUG
endif

CXXFLAGS  = --std=gnu++14 -Wall -Wextra $(DISABLED_WARNINGS) $(PIC_FLAG) $(INCLUDES) $(DEFS)
LDFLAGS   = $(LIBS)
SWIGFLAGS = -python -c++ -outdir . -includeall -Iinclude $(INCLUDES) ${SWIG_DEFS}

vpath %.cc interface
vpath %.cpp src src/edf src/blocking src/blocking/linprog src/linprog src/canbus

# #### Common C++ source files ####

EDF_OBJ   = baker.o baruah.o gfb.o bcl.o bcl_iterative.o rta.o
EDF_OBJ  += ffdbf.o gedf.o gel_pl.o load.o cpu_time.o qpa.o la.o
SCHED_OBJ = sim.o schedule_sim.o
CAN_OBJ   = msgs.o can_sim.o schedule_sim.o job_completion_stats.o tardiness_stats.o
CORE_OBJ  = tasks.o
SYNC_OBJ  = sharedres.o dpcp.o mpcp.o
SYNC_OBJ += fmlp_plus.o  global-fmlp.o msrp.o
SYNC_OBJ += global-omlp.o part-omlp.o clust-omlp.o
SYNC_OBJ += rw-phase-fair.o rw-task-fair.o
SYNC_OBJ += msrp-holistic.o qpa_msrp.o
SYNC_OBJ += global-pip.o ppcp.o


# #### Targets ####

ALL = testmain _sched.so _locking.so _sim.so _cansim.so

# Compile LP-based code only if we have a solver.
ifneq ($(CPLEX_PATH)$(GLPK_PATH),)
LP_OBJ    = lp_common.o varmapperbase.o io.o lp_dflp.o lp_dpcp.o lp_mpcp.o lp_fmlp.o lp_omip.o
LP_OBJ   += lp_spinlocks.o lp_spinlock_msrp.o lp_spinlock_unordered.o
LP_OBJ   += lp_spinlock_prio.o lp_spinlock_prio_fifo.o
LP_OBJ   += lp_gfmlp.o
LP_OBJ   += lp_global.o lp_global_pip.o lp_ppcp.o lp_sa_gfmlp.o lp_global_fmlpp.o lp_prsb.o
LP_OBJ   += lp_no_progress_priority.o lp_no_progress_fifo.o
LP_OBJ   += lp_global_no.o lp_global_pi.o lp_global_rsb.o lp_global_prio.o lp_global_fifo.o
LP_OBJ   += lp_pedf_analysis.o
LP_OBJ	 += lp_pedf_spinlocks_common.o lp_pedf_msrp.o lp_pedf_fifo_preempt.o
LP_OBJ	 += lp_pedf_lockfree_common.o lp_pedf_lockfree_NP.o lp_pedf_lockfree_preempt.o
LP_OBJ   += nested_cs.o lp_spinlock_nested_fifo.o

APA_OBJ += apa_feas.o varmapperbase.o

ifneq ($(CPLEX_PATH),)
LP_SOLVER_OBJ = cplex.o cpx.o
endif

ifneq ($(GLPK_PATH),)
LP_SOLVER_OBJ = glpk.o
endif

LP_OBJ  += ${LP_SOLVER_OBJ}
APA_OBJ += ${LP_SOLVER_OBJ}

ALL += _lp_analysis.so
endif

.PHONY: all clean

all: ${ALL}

clean:
	rm -f interface/*.cc interface/*.o *.py
	rm -f *.o ${ALL}

testmain: testmain.o ${CORE_OBJ} ${EDF_OBJ} ${SYNC_OBJ} ${SCHED_OBJ} ${LP_OBJ}
	$(CXX) -o $@ $+ $(LDFLAGS)

# #### Python libraries ####

interface/%_wrap.cc: interface/%.i
	$(SWIG) $(SWIGFLAGS) -o $@ $<

interface/%_wrap.o: interface/%_wrap.cc
	$(CXX) $(CXXFLAGS) $(DEFS) $(PIC_FLAG) $(PYTHON_INC) -c -o $@ $+ $(INCLUDES)

_sched.so: ${CORE_OBJ} ${EDF_OBJ} ${APA_OBJ} interface/sched_wrap.o
	$(CXX) $(SOFLAGS) -o $@ $+ $(LDFLAGS) $(PYTHON_LIB)

_locking.so: ${CORE_OBJ} qpa.o ${SYNC_OBJ} interface/locking_wrap.o
	$(CXX) $(SOFLAGS) -o $@ $+ $(LDFLAGS) $(PYTHON_LIB)

_sim.so: ${CORE_OBJ} ${SCHED_OBJ} interface/sim_wrap.o
	$(CXX) $(SOFLAGS) -o $@ $+ $(LDFLAGS) $(PYTHON_LIB)

_cansim.so: ${CORE_OBJ} ${CAN_OBJ} interface/cansim_wrap.o
	$(CXX) $(SOFLAGS) -o $@ $+ $(LDFLAGS) $(PYTHON_LIB)

_lp_analysis.so: ${LP_OBJ} sharedres.o mpcp.o cpu_time.o interface/lp_analysis_wrap.o
	$(CXX) $(SOFLAGS) -o $@ $+ $(LDFLAGS) $(PYTHON_LIB)
