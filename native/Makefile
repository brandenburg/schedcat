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

OS := $(shell uname)

# How to link Python libraries?
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

CXXFLAGS  = -Wall -Wextra $(DISABLED_WARNINGS) -fPIC $(INCLUDES)
LDFLAGS   = $(LIBS)
SWIGFLAGS = -python -c++ -outdir . -includeall -Iinclude

ifeq ($(DEBUG),y)
CXXFLAGS += -g -DDEBUG
else
CXXFLAGS += -O2 -DNDEBUG
endif

vpath %.cc interface
vpath %.cpp src src/edf src/blocking

# #### Common C++ source files ####

EDF_OBJ   = baker.o baruah.o gfb.o bcl.o bcl_iterative.o rta.o ffdbf.o gedf.o gel_pl.o load.o cpu_time.o
SCHED_OBJ = sim.o schedule_sim.o
CORE_OBJ  = tasks.o
SYNC_OBJ  = sharedres.o dpcp.o mpcp.o
SYNC_OBJ += fmlp_plus.o  global-fmlp.o
SYNC_OBJ += global-omlp.o part-omlp.o clust-omlp.o
SYNC_OBJ += rw-phase-fair.o rw-task-fair.o

# #### Targets ####

ALL = testmain _sched.so _locking.so _sim.so

.PHONY: all clean

all: ${ALL}

clean:
	rm -f interface/*.cc interface/*.o *.py
	rm -f *.o ${ALL}

testmain: testmain.o ${CORE_OBJ} ${SYNC_OBJ} ${EDF_OBJ} ${SCHED_OBJ}
	$(CXX) -o $@ $+ $(LDFLAGS) 

# #### Python libraries ####

interface/%_wrap.cc: interface/%.i
	$(SWIG) $(SWIGFLAGS) -o $@ $<

interface/%_wrap.o: interface/%_wrap.cc
	$(CXX) -fPIC $(PYTHON_INC) -c -o $@ $+ $(INCLUDES) 

_sched.so: ${CORE_OBJ} ${EDF_OBJ} interface/sched_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS) 

_locking.so: ${SYNC_OBJ} interface/locking_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS) 

_sim.so: ${CORE_OBJ} ${SCHED_OBJ} interface/sim_wrap.o
	$(CXX) $(SOFLAGS) $(PYTHON_LIB) -o $@ $+ $(LDFLAGS) 
