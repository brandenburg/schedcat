#ifndef LINPROG_MODEL_H
#define LINPROG_MODEL_H

#include <vector>
#include <utility>
#include <set>

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

	// set of integer variables
	std::set<unsigned int> variables_integer;

	// set of binary variables
	std::set<unsigned int> variables_binary;

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

	void declare_variable_integer(unsigned int variable_index)
	{
		variables_integer.insert(variable_index);
	}

	void declare_variable_binary(unsigned int variable_index)
	{
		variables_binary.insert(variable_index);
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

	const std::set<unsigned int>& get_integer_variables() const
	{
		return variables_integer;
	}

	bool has_binary_variables() const
	{
		return !variables_binary.empty();
	}

	bool has_integer_variables() const
	{
		return !variables_integer.empty();
	}

	bool is_integer_variable(unsigned int variable_id) const
	{
		return variables_integer.find(variable_id) != variables_integer.end();
	}

	bool is_binary_variable(unsigned int variable_id) const
	{
		return variables_binary.find(variable_id) != variables_binary.end();
	}

	const std::set<unsigned int>& get_binary_variables() const
	{
		return variables_binary;
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
