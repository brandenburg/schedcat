# SchedCAT
Schedcat: the **sched**ulability test **c**ollection **a**nd **t**oolkit.

## About

Schedcat is a Python/C++ library useful for schedulability experiments. It contains several **schedulability tests** for various real-time schedulers (including partitioned and global EDF, Pfair, etc.), **partitioning heuristics**, **blocking term analysis** for real-time locking protocols (including various spinlock types, the OMLP family, the FMLP and FMLP+, the MPCP, and the DPCP), and **overhead accounting** methods.

Additionally, the library contains task set generation routines, task set serialization support, and various helper utilities that make developing schedulability experiments faster and less painful.

This code is a refactored and cleaned-up version of the schedulability experiments provided at http://www.cs.unc.edu/~bbb/diss.

## Quick Start

To get started, first clone the repository from Github.

	# get the library 
	$ git clone https://brandenburg@github.com/brandenburg/schedcat.git
	[...]

Next, compile the C++ part of schedcat. There is a top-level `Makefile` that will take care of it. Compilation works out of the box on most Linux distributions (if not, have a look at the dependencies below).

	$ cd schedcat
	$ make
	[...]

After compilation,  execute the unit test suite with `make test` or, equivalently, with `python -m tests` to check if everything works as expected.


    $ make test
    [...]
    Ran 172 tests in 2.890s

    OK


Schedcat is a Python library with a C++ core. The best way to explore the library is to play with it in the Python shell and to read the source code.

### Schedulability Tests

The key purposes of schedcat is to collect and provide schedulability tests. In the following example, a task set consisting of three tasks is tested for  schedulability under global EDF scheduling on two and three cores.

	$ python
	
	# load task model
	>>> import schedcat.model.tasks as tasks

	# load schedulability tests for global EDF
	>>> import schedcat.sched.edf as edf

	# create task set with  three implicit-deadline tasks and total utilization 2
	>>> ts = tasks.TaskSystem([tasks.SporadicTask(2,3),
	                           tasks.SporadicTask(2,3),
	                           tasks.SporadicTask(2,3)])

	
	# Is the task set hard real-time schedulable on two processors?
	>>> edf.is_schedulable(2, ts)
	False
	
	# Well, how about three processors?
	>>> edf.is_schedulable(3, ts)
	True
	
	# Do we have bounded response times on two processors?
	>>> edf.bound_response_times(2, ts)
	True
	
	# So what's the maximum tardiness (using Devi and Anderson's analysis)?
	>>> ts[0].tardiness()
	2
	>>> ts[1].tardiness()
	2
	>>> ts[2].tardiness()
	2

### Locking Protocols

What if two tasks both access a shared resource? Schedcat also contains blocking term bounds for various locking protocols. This can be integrated into schedulability tests as follows. In this example, the global *O(m)* locking protocol (OMLP) is assumed.
    
    # load resource model
    >>> import schedcat.model.resources as resources

    # load blocking bounds
    >>> import schedcat.locking.bounds as bounds

    # initialize the resource model
    >>> resources.initialize_resource_model(ts)

    # task 0 and task 1 share resource 0 for 1 time unit each
    >>> ts[0].resmodel[0].add_request(1)
    >>> ts[1].resmodel[0].add_request(1)

    # put all tasks in the same partition (global scheduling)
    >>> for t in ts: t.partition = 0

    # assign the priorities w.r.t. locking
    >>> bounds.assign_edf_locking_prios(ts)

    # inflate execution costs by blocking terms
    >>> bounds.apply_global_omlp_bounds(ts, 3)

    # test schedulability
    >>> edf.is_schedulable(3, ts)
    True


## Next Steps

Schedcat provides reusable components for schedulability experiments, but does not itself provide a specific schedulability experiment setup. That is, how task sets are generated and tested is left to the user since there is no single right way to do it.

To use schedcat as a library in your schedulability experiments, 
simply add the `schedcat` directory that is part of the repository to the `PYTHONPATH` environment variable.

For example, under `bash`, assuming that you cloned this repository to `${HOME}/work/schedcat`, add the following line to your environment: 

    export PYTHONPATH=${HOME}/work/schedcat/schedcat:$PYTHONPATH

(Note that "schedcat" occurs twice in the path, once for the repository and once for the module.)

Alternatively, you can simply create a symlink from your experiment directory to the checked-out repository.

## Dependencies & Compilation

Python 2.7 is required. Schedcat further depends on the [GNU arbitrary precision library GMP](http://gmplib.org/) (most recent versions, including 4.3.2 and 5.0.4, work just fine) and the GMP C++ wrapper. During compilation, [Swig](http://www.swig.org/) is needed to generate a Python API for the C++ core (Swig 2.0.4 and Swig 1.3.40 are known to work).

### Linux

Schedcat is known to work on both GNU/Linux and Mac OS X (both Snow Leopard and Lion). On Linux, it should compile "out of the box" if the GMP library is installed in `/usr/lib`, where most package managers place it by default.

Schedcat has been tested with `g++ 4.4.5` on Debian "Squeeze" with the  `libgmp3c2`, `libgmp3-dev`, `libgmpxx4ldbl`, `swig`, and `g++-4.4`  packages installed.

### Mac OS X

The [Homebrew package manager](http://mxcl.github.com/homebrew/) provides the easiest way to install GNU GMP and Swig under Mac OS X.

Depending on where the GMP library is installed, you may have to provide a `.config` file in `schedcat/native` to provide the path to the GMP library in `GMP_PATH`. The `.config` file is read by the `Makefile` to override the default settings, which use the system Python and assume that the GMP library can be found in `/usr/lib`.

For example, suppose the Homebrew installation is located at `/brew` and Python 2.7.2 from Homebrew is used. A corresponding `.config` file would look as follows.

    GMP_PATH = /brew

    PYTHON_INC = -I /brew/Cellar/python/2.7.2/Frameworks/Python.framework/Versions/2.7/include/python2.7
    PYTHON_LIB = -F/brew/Cellar/python/2.7.2/Frameworks -framework Python


Have a look at the `Makefile` in `schedcat/native` for further details.

Schedcat has been tested on Mac OS X 10.7 with both Apple's LLVM-based g++ 4.2.1 and the new clang++ 2.1, using Swig 2.0.4 and GMP 5.0.4 from Homebrew.


## Limitations

At this point, the library is largely undocumented. However, the library consists of (mostly) clean Python code; it is thus not too difficult to understand the code. 

The unit test suite could use a lot more tests.

There is currently no `setup.py`, and hence also no support for `virtualenv`.

Python 3 is not supported at the moment.

## Contributions

Any improvements, bugfixes, or additional unit tests are very welcome! Please send pull requests or patches to [Björn Brandenburg](http://www.mpi-sws.org/~bbb).
