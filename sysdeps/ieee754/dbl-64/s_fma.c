/* Compute x * y + z as ternary operation.
   Copyright (C) 2010 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jakub Jelinek <jakub@redhat.com>, 2010.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <float.h>
#include <math.h>
#include <fenv.h>
#include <ieee754.h>

/* This implementation uses rounding to odd to avoid problems with
   double rounding.  See a paper by Boldo and Melquiond:
   http://www.lri.fr/~melquion/doc/08-tc.pdf  */

double
__fma (double x, double y, double z)
{
  union ieee754_double u, v, w;
  int adjust = 0;
  u.d = x;
  v.d = y;
  w.d = z;
  if (__builtin_expect (u.ieee.exponent + v.ieee.exponent
			>= 0x7ff + IEEE754_DOUBLE_BIAS - DBL_MANT_DIG, 0)
      || __builtin_expect (u.ieee.exponent >= 0x7ff - DBL_MANT_DIG, 0)
      || __builtin_expect (v.ieee.exponent >= 0x7ff - DBL_MANT_DIG, 0)
      || __builtin_expect (w.ieee.exponent >= 0x7ff - DBL_MANT_DIG, 0))
    {
      /* If x or y or z is Inf/NaN or if fma will certainly overflow,
	 compute as x * y + z.  */
      if (u.ieee.exponent == 0x7ff
	  || v.ieee.exponent == 0x7ff
	  || w.ieee.exponent == 0x7ff
	  || u.ieee.exponent + v.ieee.exponent
	     > 0x7ff + IEEE754_DOUBLE_BIAS)
	return x * y + z;
      if (u.ieee.exponent + v.ieee.exponent
	  >= 0x7ff + IEEE754_DOUBLE_BIAS - DBL_MANT_DIG)
	{
	  /* Compute 1p-53 times smaller result and multiply
	     at the end.  */
	  if (u.ieee.exponent > v.ieee.exponent)
	    u.ieee.exponent -= DBL_MANT_DIG;
	  else
	    v.ieee.exponent -= DBL_MANT_DIG;
	  /* If x + y exponent is very large and z exponent is very small,
	     it doesn't matter if we don't adjust it.  */
	  if (w.ieee.exponent > DBL_MANT_DIG)
	    w.ieee.exponent -= DBL_MANT_DIG;
	  adjust = 1;
	}
      else if (w.ieee.exponent >= 0x7ff - DBL_MANT_DIG)
	{
	  /* Similarly.
	     If z exponent is very large and x and y exponents are
	     very small, it doesn't matter if we don't adjust it.  */
	  if (u.ieee.exponent > v.ieee.exponent)
	    {
	      if (u.ieee.exponent > DBL_MANT_DIG)
		u.ieee.exponent -= DBL_MANT_DIG;
	    }
	  else if (v.ieee.exponent > DBL_MANT_DIG)
	    v.ieee.exponent -= DBL_MANT_DIG;
	  w.ieee.exponent -= DBL_MANT_DIG;
	  adjust = 1;
	}
      else if (u.ieee.exponent >= 0x7ff - DBL_MANT_DIG)
	{
	  u.ieee.exponent -= DBL_MANT_DIG;
	  if (v.ieee.exponent)
	    v.ieee.exponent += DBL_MANT_DIG;
	  else
	    v.d *= 0x1p53;
	}
      else
	{
	  v.ieee.exponent -= DBL_MANT_DIG;
	  if (u.ieee.exponent)
	    u.ieee.exponent += DBL_MANT_DIG;
	  else
	    u.d *= 0x1p53;
	}
      x = u.d;
      y = v.d;
      z = w.d;
    }
  /* Multiplication m1 + m2 = x * y using Dekker's algorithm.  */
#define C ((1 << (DBL_MANT_DIG + 1) / 2) + 1)
  double x1 = x * C;
  double y1 = y * C;
  double m1 = x * y;
  x1 = (x - x1) + x1;
  y1 = (y - y1) + y1;
  double x2 = x - x1;
  double y2 = y - y1;
  double m2 = (((x1 * y1 - m1) + x1 * y2) + x2 * y1) + x2 * y2;

  /* Addition a1 + a2 = z + m1 using Knuth's algorithm.  */
  double a1 = z + m1;
  double t1 = a1 - z;
  double t2 = a1 - t1;
  t1 = m1 - t1;
  t2 = z - t2;
  double a2 = t1 + t2;

  fenv_t env;
  feholdexcept (&env);
  fesetround (FE_TOWARDZERO);
  /* Perform m2 + a2 addition with round to odd.  */
  u.d = a2 + m2;
  if ((u.ieee.mantissa1 & 1) == 0 && u.ieee.exponent != 0x7ff)
    u.ieee.mantissa1 |= fetestexcept (FE_INEXACT) != 0;
  feupdateenv (&env);

  /* Add that to a1.  */
  a1 = a1 + u.d;

  /* And adjust exponent if needed.  */
  if (__builtin_expect (adjust, 0))
    a1 *= 0x1p53;

  return a1;
}
#ifndef __fma
weak_alias (__fma, fma)
#endif

#ifdef NO_LONG_DOUBLE
strong_alias (__fma, __fmal)
weak_alias (__fmal, fmal)
#endif