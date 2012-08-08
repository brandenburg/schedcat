
# default tools
PYTHON ?= python
SCONS  ?= scons
ETAGS  ?= etags

.PHONY: all cpp clean clean-links test links

all: links

cpp:
	$(MAKE) -C native -j

links: clean-links cpp
	cd schedcat/sched; ln -s ../../native/_sched.so; ln -s ../../native/sched.py native.py
	cd schedcat/locking; ln -s ../../native/_locking.so; ln -s ../../native/locking.py native.py
	cd schedcat/sim; ln -s ../../native/_sim.so; ln -s ../../native/sim.py native.py

clean-links:
	cd schedcat/sched; rm -f _sched.so native.py
	cd schedcat/locking;  rm -f _locking.so native.py
	cd schedcat/sim;  rm -f _sim.so native.py;

clean: clean-links
	find . -iname '*.py[oc]' -exec rm '{}' ';'
	rm -rf TAGS tags native/config.log
	$(MAKE) -C native clean

# run unit test suite
test:
	@echo "=== Running unit tests"
	$(PYTHON) -m tests

# Emacs Tags
TAGS:
	find . -type f -and  -iname '*.py' | xargs ${ETAGS}
	find cpp/include -type f -and  -iname '*.h' | xargs ${ETAGS} -a
	find cpp/src -type f -and  -iname '*.cpp' | xargs ${ETAGS} -a
	${ETAGS} -l python -a run_exp

# Vim Tags
tags:
	find . -type f -and  -iname '*.py' | xargs ctags
	find cpp/include -type f -and  -iname '*.h' | xargs ctags -a
	find cpp/src -type f -and  -iname '*.cpp' | xargs ctags -a
	ctags --language-force=Python -a run_exp
