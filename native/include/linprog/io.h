#ifndef LINPROG_IO_H
#define LINPROG_IO_H

#include <ostream>

#include "linprog/model.h"
#include "linprog/solver.h"
#include "lp_common.h"

std::ostream& pretty_print_linear_expression(
	std::ostream &os,
	const LinearExpression &exp,
	hashmap<unsigned int, std::string> &var_names);

std::ostream& pretty_print_linear_program(
	std::ostream &os,
	const LinearProgram &lp,
	hashmap<unsigned int, std::string> &var_names);

std::ostream& operator<<(std::ostream &os, const LinearExpression &exp);
std::ostream& operator<<(std::ostream &os, const LinearProgram &lp);

void dump_lp_solution(
	VarMapper& vars,
	const ResourceSharingInfo& info,
	const TaskInfo& ti,
	const Solution& solution,
	std::ostream& out,
	bool show_zeros = false);

#endif
