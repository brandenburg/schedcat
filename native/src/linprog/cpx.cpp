#define IL_STD // required by CPLEX when using STL classes.

#include <assert.h>
#include <ilcplex/cplex.h>

#include "cpu_time.h"

#include "linprog/cplex.h"

class CPXSolution : public Solution
{
private:
	CPXENVptr env;
	CPXLPptr lp;

	const LinearProgram &linprog;
	const unsigned int num_cols;
	const unsigned int num_rows;
	unsigned int num_coeffs;

	double *values;
	bool solved;

	void solve_model(double var_lb, double var_ub);

	bool setup_objective(double lb, double ub);
	bool add_rows();
	bool load_coeffs();

public:
	CPXSolution(const LinearProgram &lp, unsigned int max_num_vars,
		      double var_lb = 0.0, double var_ub = 1.0);
	~CPXSolution();

	double get_value(unsigned int var) const
	{
		return values[var];
	}

	bool is_solved() const
	{
		return solved;
	}
};

CPXSolution::CPXSolution(const LinearProgram& lp, unsigned int max_num_vars,
			     double var_lb, double var_ub)
	: env(0),
	  lp(0),
	  linprog(lp),
	  num_cols(max_num_vars),
	  num_rows(lp.get_equalities().size() +
		   lp.get_inequalities().size()),
	  num_coeffs(0),
	  values(0),
	  solved(false)
{
	if (num_cols > 0)
	{
		values = new double[num_cols];
		solve_model(var_lb, var_ub);
	} else
		// Trivial case: no variables.
		solved = true;
}


void CPXSolution::solve_model(double var_lb, double var_ub)
{
	int err;

#if DEBUG_LP_OVERHEADS >= 3
	static DEFINE_CPU_CLOCK(model_costs);
	static DEFINE_CPU_CLOCK(solver_costs);
	static DEFINE_CPU_CLOCK(extract_costs);

	model_costs.start();
#endif

	env = CPXopenCPLEX(&err);

	if (!env)
		return;

	lp = CPXcreateprob(env, &err, "blocking");

	if (!lp)
		return;

	// crash when we hit an MIP-problem;
	// this is currently not correctly handled in this code
	if (linprog.has_integer_variables() ||
		linprog.has_binary_variables())
		abort();

	if (!setup_objective(var_lb, var_ub) ||
	    !add_rows() ||
	    !load_coeffs())
		return;


#if DEBUG_LP_OVERHEADS >= 3
	model_costs.stop();
	solver_costs.start();
#endif

	err = CPXlpopt(env, lp);

	if (err != 0)
		return;

#if DEBUG_LP_OVERHEADS >= 3
	solver_costs.stop();
	extract_costs.start();
#endif

	err = CPXsolution(env, lp, NULL, NULL, values, NULL, NULL, NULL);
	solved = err == 0;

#if DEBUG_LP_OVERHEADS >= 3
	extract_costs.stop();

	std::cout << model_costs << std::endl
		  << solver_costs << std::endl
		  << extract_costs << std::endl;
#endif
}

CPXSolution::~CPXSolution()
{
	int status;

	if (lp) {
		status = CPXfreeprob(env, &lp);
		assert(status == 0);
	}

	if (env) {
		status = CPXcloseCPLEX(&env);
		assert(status == 0);
	}

	delete [] values;
}

bool CPXSolution::setup_objective(double lb, double ub)
{

	const LinearExpression *obj = linprog.get_objective();
	int err;

	double *all  = new double[num_cols * 3];
	double *vals = all;
	double *lbs  = all + num_cols;
	double *ubs  = all + 2 * num_cols;

	assert(obj->get_terms().size() <= num_cols);

	for (unsigned int i = 0; i < num_cols; i++)
	{
		vals[i] = 0;
		lbs[i]  = lb;
		ubs[i]  = ub;
	}

	foreach(obj->get_terms(), term)
		vals[term->second] = term->first;

	CPXchgobjsen(env, lp, CPX_MAX);
	err = CPXnewcols(env, lp, num_cols, vals, lbs, ubs, NULL, NULL);

	delete [] all;

	return err == 0;
}

bool CPXSolution::add_rows()
{
	double *bounds = new double[num_rows];
	char   *senses = new char[num_rows];
	int err;

	unsigned int r = 0;

	foreach(linprog.get_equalities(), equ)
	{
		bounds[r] = equ->second;
		senses[r] = 'E'; // equality constraint

		num_coeffs += equ->first->get_terms().size();
		r++;
	}

	foreach(linprog.get_inequalities(), inequ)
	{
		bounds[r] = inequ->second;
		senses[r] = 'L'; // less-than-or-equal constraint

		num_coeffs += inequ->first->get_terms().size();
		r++;
	}


	err = CPXnewrows(env, lp, num_rows, bounds, senses, NULL, NULL);

	delete [] bounds;
	delete [] senses;

	return err == 0;
}

bool CPXSolution::load_coeffs()
{
	unsigned int r = 0;
	int err;

	foreach(linprog.get_equalities(), equ)
	{
		foreach(equ->first->get_terms(), term)
		{
			err = CPXchgcoef(env, lp, r, term->second, term->first);
			if (err != 0)
				return false;
		}
		r++;
	}

	foreach(linprog.get_inequalities(), inequ)
	{
		foreach(inequ->first->get_terms(), term)
		{
			err = CPXchgcoef(env, lp, r, term->second, term->first);
			if (err != 0)
				return false;
		}
		r++;
	}

	return true;
}

Solution *cpx_solve(const LinearProgram& lp, unsigned int max_num_vars)
{
	CPXSolution *sol =  new CPXSolution(lp, max_num_vars);
	if (sol->is_solved())
		return sol;
	else
	{
		delete sol;
		return NULL;
	}
}
