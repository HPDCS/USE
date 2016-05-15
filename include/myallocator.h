/*****************************************************************************
*
*	This file is part of NBQueue, a lock-free O(1) priority queue.
*
*   Copyright (C) 2015, Romolo Marotta
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/
/*
 * allocator.h
 *
 *  Created on: Nov 6, 2015
 *      Author: Romolo Marotta
 */

#ifndef MM_MYALLOCATOR_H_
#define MM_MYALLOCATOR_H_

#include <stdbool.h>
#include <sys/time.h>
#include <stddef.h>

void  mm_init(size_t, size_t, bool);
void* mm_malloc(void);
void  mm_free(void*);
void* mm_std_malloc(size_t size);
void* rsalloc(size_t size);
void  mm_std_free(void* pointer);
void  rsfree(void* pointer);
void  mm_print_log();
void mm_get_log_data(struct timeval *time_malloc, unsigned int *ops_malloc,
		struct timeval *time_free, unsigned int *ops_free);

#endif /* MM_MYALLOCATOR_H_ */
