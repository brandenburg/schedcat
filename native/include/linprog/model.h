#ifndef LINPROG_MODEL_H
#define LINPROG_MODEL_H

#include <vector>
#include <utility>

#include "stl-helper.h"

typedef std::pair<double, unsigned int> Term;
typedef std::vector<Term> Terms;

class LinearExpression
{
private:
	Terms terms;

public:
	void add_term(double coefficient, unsigned int variable_index)
	{
		terms.push_back(Term(coefficient, variable_index));
	}

	// by default, assumes coefficient == 1
	void add_var(unsigned int variable_index)
	{
		add_term(1.0, variable_index);
	}

	const Terms& get_terms(void) const
	{
		return terms;
	}

	bool has_terms(void) const
	{
		return !terms.empty();
	}
};

typedef std::pair<LinearExpression *, double> Constraint;
typedef std::vector<Constraint> Constraints;

// builds a maximization problem piece-wise
class LinearProgram
{
	// the function to be maximized
	LinearExpression *objective;

	// linear expressions constrained to an exact value
	Constraints equalities;

	// linear expressions constrained by an upper bound (exp <= bound)
	Constraints inequalities;

public:
	LinearProgram() : objective(new LinearExpression()) {};

	~LinearProgram()
	{
		delete objective;
		foreach(equalities, eq)
			delete eq->first;
		foreach(inequalities, ineq)
			delete ineq->first;
	}

	void set_objective(LinearExpression *exp)
	{
		delete objective;
		objective = exp;
	}

	void add_inequality(LinearExpression *exp, double upper_bound)
	{
		if (exp->has_terms())
			inequalities.push_back(Constraint(exp, upper_bound));
		else
			delete exp;
	}

	void add_equality(LinearExpression *exp, double equal_to)
	{
		if (exp->has_terms())
			inequalities.push_back(Constraint(exp, equal_to));
		else
			delete exp;
	}

	const LinearExpression *get_objective() const
	{
		return objective;
	}

	LinearExpression *get_objective()
	{
		return objective;
	}

	const Constraints& get_equalities() const
	{
		return equalities;
	}

	const Constraints& get_inequalities() const
	{
		return inequalities;
	}
};

#endif
