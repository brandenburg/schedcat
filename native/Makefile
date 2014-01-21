-include .config
# set DEBUG=y to disable optimizations and enable debug info
# set GMP_PATH to find dependency in non-standard location
# set SWIG to find swig binary if not in current $PATH

# #### Tools and Paths ####

# default: swig binary available in $PATH.
SWIG     ?= swig
# default: standard python in $PATH.
PYTHON   ?= python
# default: assume GMP is installed in standard location.
GMP_PATH ?= /usr

# #### Flags ####

DISABLED_WARNINGS  = -Wno-unused-parameter # Too much noise.
DISABLED_WARNINGS += -Wno-deprecated       # We still use ext::hash_map<>.

INCLUDES = -Iinclude -I${GMP_PATH}/include
LIBS     = -L${GMP_PATH}/lib -lgmp -lgmpxx
DEFS    ?=

OS := $(shell uname)

# How to link Python libraries?
PYTHON_VER := ${shell ${PYTHON} --version 2>&1}
ifeq ($(OS),Darwin)
# Mac OS X: link against Python Framework
PYTHON_INC ?= -I/System/Library/Frameworks/Python.framework/Headers/
PYTHON_LIB ?= -framework Python
SOFLAGS     = -dynamiclib
else
# Default/Linux: assume Python is in standard library path
PYTHON_INC ?= -I${shell ${PYTHON} -c "import distutils.sysconfig; print distutils.sysconfig.get_python_inc()" }
# No special linking required.
PYTHON_LIB ?=
SOFLAGS     = -shared
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
CPLEX_PATH ?= $(CPLEX_DIR)
endif

ifneq ($(CPLEX_PATH),)
${info Linking with CPLEX installation in $(CPLEX_DIR).}
# Add headers, libraries, and definitions.
INCLUDES += -I$(CPLEX_PATH)/include/
LIBS     += -lilocplex -lcplex -lconcert
DEFS     += -DCONFIG_HAVE_CPLEX
endif


# #### GLPK Support ####

GLPK := ${realpath ${shell which glpsol}}
ifneq ($(GLPK),)
GLPK_DIR :=${realpath ${dir $(GLPK)}/..}
GLPK_PATH ?= $(GLPK_DIR)
endif

ifneq ($(GLPK_PATH),)
${info Linking with GLPK installed at prefix=$(GLPK_DIR).}
INCLUDES += -I$(GLPK_PATH)/include
LIBS     += -L$(GLPK_PATH)/lib -lglpk
DEFS     += -DCONFIG_HAVE_GLPK
endif

# #### Debug support ####
ifeq ($(DEBUG),y)
DEFS += -g -DDEBUG -march=native
else
DEFS += -O2 -march=native
endif

# disable assertions only if explicitly requested
ifeq ($(DEBUG),n)
DEFS += -DNDEBUG
endif

CXXFLAGS  = -Wall -Wextra -Werror $(DISABLED_WARNINGS) -fPIC $(INCLUDES) $(DEFS)
LDFLAGS   = $(LIBS)
SWIGFLAGS = -python -c++ -outdir . -includeall -Iinclude

vpath %.cc interface
vpath %.cpp src src/edf src/blocking src/blocking/linprog src/linprog

# #### Common C++ source files ####

EDF_OBJ   = baker.o baruah.o gfb.o bcl.o bcl_iterative.o rta.o
EDF_OBJ  += ffdbf.o gedf.o gel_pl.o load.o cpu_time.o qpa.o la.o
SCHED_OBJ = sim.o schedule_sim.o
CORE_OBJ  = tasks.o
SYNC_OBJ  = sharedres.o dpcp.o mpcp.o
SYNC_OBJ += fmlp_plus.o  global-fmlp.o msrp.o
SYNC_OBJ += global-omlp.o part-omlp.o clust-omlp.o
SYNC_OBJ += rw-phase-fair.o rw-task-fair.o
SYNC_OBJ += msrp-holistic.o

# #### Targets ####

ALL = testmain _sched.so _locking.so _sim.so

# Compile LP-based code only if we have a solver.
ifneq ($(CPLEX_PATH)$(GLPK_PATH),)
LP_OBJ    = lp_common.o io.o lp_dflp.o lp_dpcp.o lp_mpcp.o lp_fmlp.o lp_omip.o
LP_OBJ   += lp_spinlocks.o lp_spinlock_msrp.o lp_spinlock_unordered.o
LP_OBJ   += lp_spinlock_prio.o lp_spinlock_prio_fifo.o
LP_OBJ   += lp_gfmlp.o

ifneq ($(CPLEX_PATH),)
LP_OBJ   += cplex.o cpx.o
endif

ifneq ($(GLPK_PATH),)
LP_OBJ   += glpk.o
endif

ALL += _lp_analysis.so
endif

.PHONY: all clean

all: ${ALL}

clean:
	rm -f interface/*.cc interface/*.o *.py
	rm -f *.o ${ALL}

testmain: testmain.o ${CORE_OBJ} ${SYNC_OBJ} ${EDF_OBJ} ${SCHED_OBJ} ${LP_OBJ}
	$(CXX) -o $@ $+ $(LDFLAGS)

# #### Python libraries ####

interface/%_wrap.cc: interface/%.i
	$(SWIG) $(SWIGFLAGS) -o $@ $<

interface/%_wrap.o: interface/%_wrap.cc
	$(CXX) $(DEFS) -fPIC $(PYTHON_INC) -c -o $@ $+ $(INCLUDES)

_sched.so: ${CORE_OBJ} ${EDF_OBJ} interface/sched_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS)

_locking.so: ${SYNC_OBJ} interface/locking_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS)

_sim.so: ${CORE_OBJ} ${SCHED_OBJ} interface/sim_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS)

_lp_analysis.so: ${LP_OBJ} sharedres.o mpcp.o cpu_time.o interface/lp_analysis_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS)
