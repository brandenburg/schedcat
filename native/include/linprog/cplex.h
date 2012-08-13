#ifndef LINPROG_CPLEX_H
#define LINPROG_CPLEX_H

#include "linprog/solver.h"

// solve with CPLEX connected via the "Concert Technology" API
Solution *cplex_solve(const LinearProgram& lp, unsigned int max_num_vars);

// solve with CPLEX connected via the plain, old C API
Solution *cpx_solve(const LinearProgram& lp, unsigned int max_num_vars);

#endif
