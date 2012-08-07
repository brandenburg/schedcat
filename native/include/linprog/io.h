#ifndef LINPROG_IO_H
#define LINPROG_IO_H

#include <ostream>

#include "linprog/model.h"

std::ostream& operator<<(std::ostream &os, const LinearExpression &exp);

#endif
