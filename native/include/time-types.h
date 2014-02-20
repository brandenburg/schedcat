#ifndef TIME_TYPES_H
#define TIME_TYPES_H

/* include string.h for gmpxx.h */
#include <string.h>
#include <gmpxx.h>

typedef mpz_class integral_t;
typedef mpq_class fractional_t;

static inline void truncate_fraction(fractional_t &val)
{
    val.get_num() -= val.get_num() % val.get_den();
    val.canonicalize();
}

#endif
