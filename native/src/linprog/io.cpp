#include <iostream>

#include "stl-hashmap.h"

#include "linprog/solver.h"
#include "linprog/io.h"

std::ostream& pretty_print_linear_expression(
	std::ostream &os,
	const LinearExpression &exp,
	hashmap<unsigned int, std::string> &var_names)
{
	bool first = true;
	foreach (exp.get_terms(), term)
	{
		if (term->first == -1)
			os << "- ";
		else if (term->first < 0)
			os << "- " << -term->first << " ";
		else if (!first && term->first == 1)
			os << "+ ";
		else if (!first)
			os << "+ " << term->first << " ";
		else if (term->first != 1)
			os << term->first << " ";

		if (var_names.find(term->second) != var_names.end())
			os << var_names[term->second] << " ";
		else
			os <<  "X" << term->second << " ";
		first = false;
	}

	return os;
}

std::ostream& operator<<(std::ostream &os, const LinearExpression &exp)
{
	hashmap<unsigned int, std::string> dummy_map;
	return pretty_print_linear_expression(os, exp, dummy_map);
}

std::ostream& pretty_print_linear_program(
	std::ostream &os,
	const LinearProgram &lp,
	hashmap<unsigned int, std::string> &var_names)
{
	os << "maximize ";
	pretty_print_linear_expression(os, *lp.get_objective(), var_names);
	os << " subject to:" << std::endl;
	foreach (lp.get_equalities(), it)
	{
		pretty_print_linear_expression(os, *(it->first), var_names);
		os << " = " << it->second << std::endl;
	}
	foreach (lp.get_inequalities(), it)
	{
		pretty_print_linear_expression(os, *(it->first), var_names);
		os << " <= " << it->second << std::endl;
	}

	return os;
}

std::ostream& operator<<(std::ostream &os, const LinearProgram &lp)
{
	hashmap<unsigned int, std::string> dummy_map;
	return pretty_print_linear_program(os, lp, dummy_map);
}

void dump_lp_solution(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	const Solution& solution,
	std::ostream& out,
	bool show_zeros)
{
	foreach_task_except(info.get_tasks(), ti, tx)
	{
		unsigned int t = tx->get_id();

		out << "T" << t << " part=" << tx->get_cluster() << std::endl;

		foreach(tx->get_requests(), request)
		{
			unsigned int q = request->get_resource_id();

			out << "  res=" << q
			    << "  L=" << request->get_request_length()
			    << std::endl;

			foreach_request_instance(*request, ti, v)
			{
				unsigned int var_id;
				bool newline = false;

				var_id = vars.lookup(t, q, v, BLOCKING_DIRECT);
				if (solution.get_value(var_id) || show_zeros)
				{
					out << "    XD_" << t  << "_" << q << "_" << v
					    << "=" << solution.get_value(var_id);
					newline = true;
				}

				var_id = vars.lookup(t, q, v, BLOCKING_INDIRECT);
				if (solution.get_value(var_id) || show_zeros)
				{
					out << "    XI_" << t  << "_" << q << "_" << v
					    << "=" << solution.get_value(var_id);
					newline = true;
				}


				var_id = vars.lookup(t, q, v, BLOCKING_PREEMPT);
				if (solution.get_value(var_id) || show_zeros)
				{
					out << "    XP_" << t  << "_" << q << "_" << v
					    << "=" << solution.get_value(var_id);
					newline = true;
				}

				if (newline)
					out << std::endl;
			}
		}
	}
}

