#ifndef MATH_HELPER_H
#define MATH_HELPER_H

static inline unsigned long divide_with_ceil(unsigned long numer,
					     unsigned long denom)
{
	if (numer % denom == 0)
		return numer / denom;
	else
		/* integer division computes implicit floor */
		return (numer / denom) + 1;
}

#endif
