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



#endif
