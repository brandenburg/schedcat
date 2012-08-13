#ifndef LINPROG_SOLVER_H
#define LINPROG_SOLVER_H

#include "linprog/model.h"

class Solution
{

public:
	virtual ~Solution() {};

	virtual double get_value(unsigned int variable_index) const = 0;

	virtual double evaluate(const LinearExpression &exp) const
	{
		double sum = 0;
		foreach(exp.get_terms(), term)
		{
			double coeff = term->first;
			double var   = term->second;
			sum += coeff * get_value(var);
		}
		return sum;
	}
};

#if defined(CONFIG_HAVE_CPLEX)

#include "linprog/cplex.h"
#define linprog_solve(lp, vars) cpx_solve((lp), (vars))

#elif defined(CONFIG_HAVE_GLPK)

#include "linprog/glpk.h"
#define linprog_solve(lp, vars) glpk_solve((lp), (vars))

#else

#warning No LP solver available.
#define linprog_solve(lp, vars) assert(0)

#endif

#endif
