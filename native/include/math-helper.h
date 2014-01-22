#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include "time-types.h"

static inline unsigned long divide_with_ceil(unsigned long numer,
					     unsigned long denom)
{
	if (numer % denom == 0)
		return numer / denom;
	else
		/* integer division computes implicit floor */
		return (numer / denom) + 1;
}




static inline integral_t divide_with_ceil(const integral_t &numer,
					  const integral_t &denom)
{
	integral_t result;
	mpz_cdiv_q(result.get_mpz_t(), numer.get_mpz_t(), denom.get_mpz_t());
	return result;
}


static inline integral_t round_up(const fractional_t &f)
{
	integral_t result;
	mpz_cdiv_q(result.get_mpz_t(), f.get_num_mpz_t(), f.get_den_mpz_t());
	return result;
}


#endif
