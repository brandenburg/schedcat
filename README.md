# SchedCAT
Schedcat: the **sched**ulability test **c**collection **a**nd **t**oolkit.

## About

Schedcat is a Python/C++ library useful for schedulability experiments. It contains several **schedulability tests** for various real-time schedulers (including partitioned and global EDF, Pfair, etc.), **partitioning heuristics**, **blocking term analysis** for real-time locking protocols (including various spinlock types, the OMLP family, the FMLP and FMLP+, the MPCP, and the DPCP), and **overhead accounting** methods.

Additionally, the library contains task set generation routines, task set serialization support, and various helper utilities that make developing schedulability experiments faster and less painful.

This code is a refactored and cleaned-up version of the schedulability experiments provided at http://www.cs.unc.edu/~bbb/diss.

## Quick Start

	# get the library 
	$ git clone ...
	[...]
	$ cd schedcat

	# compile (works out of the box on most Linux distributions)
	$ make
	[...]
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

To check if everything works as expected, you can execute the unit test suite with `make test` or, equivalently, with `python -m tests`.


## Dependencies

Python 2.7 is required. Schedcat further requires the [GNU arbitrary precision library GMP](http://gmplib.org/) (most recent versions, including 5.0.4, work just fine) and the GMP C++ wrapper.

Schedcat is known to work on both GNU/Linux and Mac OS X (both Snow Leopard and Lion). On Linux, it should compile "out of the box" if the GMP library is installed in `/usr/lib`, where most package managers place it by default.

On Mac OS X, depending on where the GMP library is installed, you may have to provide a `.config` file in `schedcat/native` to provide the path to the GMP library in `GMP_PATH` (have a look at the `Makefile` in `schedcat/native` for details).

The [Homebrew package manager](http://mxcl.github.com/homebrew/) provides the easiest way to install GNU GMP under Mac OS X.

## Limitations

At this point, the library is largely undocumented. However, the library consists of (mostly) clean Python code; it is thus not too difficult to understand the code. 

The unit test suite could use a lot more tests.

## Contributions

Improvements, bugfixes, additional unit tests are very welcome! Please send pull requests or patches to [Björn Brandenburg](http://www.mpi-sws.org/~bbb).
