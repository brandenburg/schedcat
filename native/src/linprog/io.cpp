#include <iostream>

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
