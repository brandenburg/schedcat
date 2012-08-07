#ifndef LINPROG_CPLEX_H
#define LINPROG_CPLEX_H

#include "linprog/solver.h"

Solution *cplex_solve(const LinearProgram& lp, unsigned int max_num_vars);

#endif
