/* Copy of /usr/include/assert.h, renamed myassert, and NDEBUG doesn't
 * disable me.
 */


/* Copyright (C) 1991,1992,1994-2001,2003,2004,2007
   Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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

/*
 *	ISO C99 Standard: 7.2 Diagnostics	<myassert.h>
 */

#ifdef	_MYASSERT_H

# undef	_MYASSERT_H
# undef	myassert
# undef __MYASSERT_VOID_CAST

# ifdef	__USE_GNU
#  undef myassert_perror
# endif

#endif /* myassert.h	*/

#define	_MYASSERT_H	1
#include <features.h>

#if defined __cplusplus && __GNUC_PREREQ (2,95)
# define __MYASSERT_VOID_CAST static_cast<void>
#else
# define __MYASSERT_VOID_CAST (void)
#endif

/* void myassert (int expression);

   If NDEBUG is defined, do nothing.
   If not, and EXPRESSION is zero, print an error message and abort.  */

#ifndef _MYASSERT_H_DECLS
#define _MYASSERT_H_DECLS
__BEGIN_DECLS

/* This prints an "Myassertion failed" message and aborts.  */
extern void __assert_fail (__const char *__myassertion, __const char *__file,
			   unsigned int __line, __const char *__function)
     __THROW __attribute__ ((__noreturn__));

/* Likewise, but prints the error text for ERRNUM.  */
extern void __assert_perror_fail (int __errnum, __const char *__file,
				  unsigned int __line,
				  __const char *__function)
     __THROW __attribute__ ((__noreturn__));


/* The following is not at all used here but needed for standard
   compliance.  */
extern void __assert (const char *__myassertion, const char *__file, int __line)
     __THROW __attribute__ ((__noreturn__));


__END_DECLS
#endif /* Not _MYASSERT_H_DECLS */

# define myassert(expr)							\
  ((expr)								\
   ? __MYASSERT_VOID_CAST (0)						\
   : __assert_fail (__STRING(expr), __FILE__, __LINE__, __MYASSERT_FUNCTION))

# ifdef	__USE_GNU
#  define myassert_perror(errnum)						\
  (!(errnum)								\
   ? __MYASSERT_VOID_CAST (0)						\
   : __assert_perror_fail ((errnum), __FILE__, __LINE__, __MYASSERT_FUNCTION))
# endif

/* Version 2.4 and later of GCC define a magical variable `__PRETTY_FUNCTION__'
   which contains the name of the function currently being defined.
   This is broken in G++ before version 2.6.
   C9x has a similar variable called __func__, but prefer the GCC one since
   it demangles C++ function names.  */
# if defined __cplusplus ? __GNUC_PREREQ (2, 6) : __GNUC_PREREQ (2, 4)
#   define __MYASSERT_FUNCTION	__PRETTY_FUNCTION__
# else
#  if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#   define __MYASSERT_FUNCTION	__func__
#  else
#   define __MYASSERT_FUNCTION	((__const char *) 0)
#  endif
# endif
