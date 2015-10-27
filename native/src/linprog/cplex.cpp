#define IL_STD // required by CPLEX when using STL classes.

#include <ilcplex/ilocplex.h>

#include "cpu_time.h"

#include "linprog/cplex.h"

class CPLEXSolution : public Solution
{
private:
	IloEnv ilo_env;
	IloEnv& get_env()
	{
		return ilo_env;
	}

	IloNumArray	cplex_values;

	const LinearProgram &linprog;

	bool solved;

	void solve_model(unsigned int max_num_vars,
			 double var_lb, double var_ub);

	IloObjective make_objective(const IloNumVarArray& vars);
        IloRangeArray make_constraints(const IloNumVarArray& vars);

	IloRange make_constraint(const IloNumVarArray &vars,
				 const LinearExpression *exp, double bound,
				 bool is_exact_bound);

public:
	CPLEXSolution(const LinearProgram &lp, unsigned int max_num_vars,
		      double var_lb = 0.0, double var_ub = 1.0);
	~CPLEXSolution();

	double get_value(unsigned int var) const
	{
		return cplex_values[var];
	}

	bool is_solved() const
	{
		return solved;
	}
};

CPLEXSolution::CPLEXSolution(const LinearProgram& lp, unsigned int max_num_vars,
			     double var_lb, double var_ub)
	: ilo_env(),
	  cplex_values(get_env(), max_num_vars),
	  linprog(lp),
	  solved(false)
{
	get_env().setNormalizer(IloFalse);
	get_env().setOut(get_env().getNullStream());

	if (max_num_vars > 0)
		solve_model(max_num_vars, var_lb, var_ub);
	else
		// Trivial case: no variables.
		solved = true;
}


void CPLEXSolution::solve_model(unsigned int max_num_vars,
				double var_lb, double var_ub)
{
	try
	{
		// This implementation currently doesn't deal correctly with non-default bounds.
		if (!linprog.get_non_default_variable_ranges().empty())
			abort(); // unsupported configuration

#if DEBUG_LP_OVERHEADS >= 3
		static DEFINE_CPU_CLOCK(model_costs);
		static DEFINE_CPU_CLOCK(solver_costs);
		static DEFINE_CPU_CLOCK(extract_costs);

		model_costs.start();
#endif
		IloNumVarArray cplex_vars = IloNumVarArray(get_env(), 0);
		for (unsigned int var_id = 0; var_id < max_num_vars; var_id++)
		{
			IloNumVar IloVar;
			if (linprog.is_binary_variable(var_id))
				IloVar = IloNumVar(get_env(), var_lb, var_ub, IloNumVar::Bool);
			else if (linprog.is_integer_variable(var_id))
				IloVar = IloNumVar(get_env(), var_lb, IloIntMax, IloNumVar::Int);
			else
				IloVar = IloNumVar(get_env(), var_lb, var_ub, IloNumVar::Float);
			cplex_vars.add(IloVar);
		}

		IloObjective objective = make_objective(cplex_vars);
		IloRangeArray constraints = make_constraints(cplex_vars);

		IloModel model = IloModel(get_env());

		model.add(objective);
		model.add(constraints);


		IloCplex cplex = IloCplex(model);

#if DEBUG_LP_OVERHEADS >= 3
		model_costs.stop();
		solver_costs.start();
#endif

		// The primal solver seems to be slightly faster.
		cplex.setParam(IloCplex::RootAlg, IloCplex::Primal);
		cplex.setParam(IloCplex::PreDual, -1);
		cplex.solve();

#if DEBUG_LP_OVERHEADS >= 3
		solver_costs.stop();
		extract_costs.start();
#endif

		cplex.getValues(cplex_vars, cplex_values);
		solved = true;

#if DEBUG_LP_OVERHEADS >= 3
		extract_costs.stop();

		std::cout << model_costs << std::endl
			  << solver_costs << std::endl
			  << extract_costs << std::endl;
#endif

	} catch (IloException &ex)
	{
		// Improve me: we should export some kind of error info.
		std::cerr << "CPLEX failed: " << ex << std::endl;
		solved = false;
	}
}

IloObjective CPLEXSolution::make_objective(const IloNumVarArray &vars)
{
	const LinearExpression *obj = linprog.get_objective();

	IloObjective goal = IloObjective(get_env(), 0,  IloObjective::Maximize);
	IloNumArray coeffs = IloNumArray(get_env(), vars.getSize());

	assert((int) obj->get_terms().size() <= coeffs.getSize());

	coeffs.add(vars.getSize(), 0);

	// Set coefficient for each variable in the objective function.
	foreach(obj->get_terms(), term)
	{
		double coefficient   = term->first;
		unsigned int var_idx = term->second;
		coeffs[var_idx] = coefficient;
	}

	goal.setLinearCoefs(vars, coeffs);

	return goal;
}

IloRangeArray CPLEXSolution::make_constraints(const IloNumVarArray &vars)
{
	IloRangeArray constraints(get_env());

	foreach (linprog.get_equalities(), exp)
		constraints.add(make_constraint(vars, exp->first, exp->second, true));

	foreach (linprog.get_inequalities(), exp)
		constraints.add(make_constraint(vars, exp->first, exp->second, false));

	return constraints;
}

IloRange CPLEXSolution::make_constraint(const IloNumVarArray &vars,
					const LinearExpression *exp, double bound,
					bool is_exact_bound)
{
	IloRange r = IloRange(get_env(), -IloInfinity, bound);

	if (is_exact_bound)
		r.setLB(bound);

	foreach (exp->get_terms(), term)
		r.setLinearCoef(vars[term->second], term->first);

	return r;
}

CPLEXSolution::~CPLEXSolution()
{
	get_env().end();
}


Solution *cplex_solve(const LinearProgram& lp, unsigned int max_num_vars)
{
	CPLEXSolution *sol =  new CPLEXSolution(lp, max_num_vars);
	if (sol->is_solved())
		return sol;
	else
	{
		delete sol;
		return NULL;
	}
}
