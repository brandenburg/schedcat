#ifndef LINPROG_CPLEX_H
#define LINPROG_CPLEX_H

#include "linprog/model.h"

class Solution;

// solve with CPLEX connected via the "Concert Technology" API
Solution *cplex_solve(const LinearProgram& lp, unsigned int max_num_vars);

// solve with CPLEX connected via the plain, old C API
Solution *cpx_solve(const LinearProgram& lp, unsigned int max_num_vars);

#include "linprog/solver.h"

#endif
