/**
*			Copyright (C) 2008-2013 HPDC Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file atomic.c
* @brief This module implements atomic and non-blocking operations used within ROOT-Sim
*        to coordinate threads and processes (on shared memory)
* @author Alessandro Pellegrini
* @date Jan 25, 2012
*/


// Do not compile anything here if we're not on an x86 machine!
#if defined(ARCH_X86) || defined(ARCH_X86_64)


#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "atomic.h"



/**
* This function implements the atomic_test_and_set on an integer value, for x86-64 archs
*
* @author Alessandro Pellegrini
*
* @param b the counter there to perform the operation
*
* @ret true if the int value has been set, false otherwise
*/
inline int atomic_bit_test_and_set_x86(unsigned short *b, int pos) {
    int result = 0;

	__asm__  __volatile__ (
		LOCK "bts %2, %1;\n\t"
		"adc %0, %0"
		: "=r" (result)
		: "m" (*b), "r" (pos), "0" (result)
		: "memory"
	);

	return !result;
}


/**
* This function implements (on x86-64 architectures) the atomic inc operation.
* It increments the atomic counter 'v' by 1 unit
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_inc_x86(atomic_t *v) {
	__asm__ __volatile__(
		LOCK "incl %0"
		: "=m" (v->count)
		: "m" (v->count)
	);
}



/**
* This function implements (on x86-64 architectures) the atomic dec operation.
* It decrements the atomic counter 'v' by 1 unit
*
* @author Alessandro Pellegrini
*
* @param v the atomic counter which is the destination of the operation
*/
inline void atomic_dec_x86(atomic_t *v) {
	__asm__ __volatile__(
		LOCK "decl %0"
		: "=m" (v->count)
		: "m" (v->count)
	);
}




#endif /* defined(ARCH_X86) || defined(ARCH_X86_64) */
