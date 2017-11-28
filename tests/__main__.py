#!/usr/bin/env python

import unittest

import tests.model
import tests.util
import tests.generator
import tests.quanta
import tests.pfair
import tests.edf
import tests.fp
import tests.fp_blocking
import tests.binpack
import tests.locking
import tests.global_locking_analysis
import tests.sim
import tests.overheads
import tests.canbus
import tests.example_end_to_end
import tests.apa
import tests.sched

suite = unittest.TestSuite(
    [unittest.defaultTestLoader.loadTestsFromModule(x) for x in
     [tests.model,
      tests.util,
      tests.generator,
      tests.quanta,
      tests.pfair,
      tests.edf,
      tests.fp,
      tests.fp_blocking,
      tests.binpack,
      tests.locking,
      tests.global_locking_analysis,
      tests.sim,
      tests.overheads,
      tests.canbus,
      tests.apa,
      tests.example_end_to_end,
      tests.sched]
    ])

def run_all_tests():
    unittest.TextTestRunner(verbosity=2).run(suite)

if __name__ == '__main__':
    run_all_tests()
