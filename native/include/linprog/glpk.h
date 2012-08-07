#ifndef LINPROG_GLPK_H
#define LINPROG_GLPK_H

#include "linprog/solver.h"

Solution *glpk_solve(const LinearProgram& lp, unsigned int max_num_vars);

#endif
