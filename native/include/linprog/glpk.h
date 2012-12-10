#ifndef LINPROG_GLPK_H
#define LINPROG_GLPK_H

#include "linprog/model.h"

class Solution;

Solution *glpk_solve(const LinearProgram& lp, unsigned int max_num_vars);

#include "linprog/solver.h"

#endif
