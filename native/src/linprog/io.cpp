#include <iostream>

#include "linprog/solver.h"
#include "linprog/io.h"

std::ostream& operator<<(std::ostream &os, const LinearExpression &exp)
{
	bool first = true;
	foreach (exp.get_terms(), term)
	{
		if (term->first < 0)
			os << "- " << -term->first;
		else if (!first)
			os << "+ " << term->first;
		else
			os << term->first;

		os <<  " X" << term->second << " ";
		first = false;
	}

	return os;
}

std::ostream& operator<<(std::ostream &os, const LinearProgram &lp)
{
	os << "maximize " << *lp.get_objective() << " subject to:" << std::endl;
	foreach (lp.get_equalities(), it)
	{
		os << *(it->first) << " = " << it->second << std::endl;
	}
	foreach (lp.get_inequalities(), it)
	{
		os << *(it->first) << " <= " << it->second << std::endl;
	}

	return os;
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

