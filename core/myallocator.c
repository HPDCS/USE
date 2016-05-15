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
 * allocator.c
 *
 *  Created on: Nov 6, 2015
 *      Author: Romolo Marotta
 */



#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>

#include "myallocator.h"


static bool active = false;
static size_t block_size = 0;
static size_t item_size = 0;
volatile unsigned long long * volatile hashtable[32];
volatile size_t free_index = 0;

__thread struct timeval mm_malloc_time;
__thread struct timeval mm_free_time;
__thread unsigned int mm_count_malloc = 0;
__thread unsigned int mm_count_free = 0;



void mm_init(size_t b_size, size_t i_size, bool activate)
{
	int i;
	for (i=0;i<32;i++)
		hashtable[i] = NULL;
	active = activate;
	block_size = 1;
	item_size = i_size;

	if(activate)
	{
		item_size += sizeof(unsigned long long*);
		block_size = b_size;
	}
	//printf("MY MALLOC %u %u %u %u\n", block_size, item_size, block_size*item_size, activate);

}

void mm_get_log_data(struct timeval *time_malloc, unsigned int *ops_malloc,
		struct timeval *time_free, unsigned int *ops_free)
{
	*time_malloc = mm_malloc_time;
	*ops_malloc = mm_count_malloc;
	*time_free = mm_free_time;
	*ops_free = mm_count_free;

}

void mm_print_log()
{
	printf("MALLOC time: %d:%d, ops:%d\n", (int)mm_malloc_time.tv_sec, (int)mm_malloc_time.tv_usec, mm_count_malloc);
	printf("FREE time: %d:%d, ops:%d\n",   (int)mm_free_time.tv_sec, (int)mm_free_time.tv_usec, mm_count_free);
}

void* mm_malloc(void)
{
//		struct timeval startTV,endTV,diff;
//		gettimeofday(&startTV, NULL);
		void* res = malloc(item_size*block_size);
//		gettimeofday(&endTV, NULL);
//		timersub(&endTV, &startTV, &diff);
//		timeradd(&diff, &mm_malloc_time, &mm_malloc_time);
		mm_count_malloc++;
		return res;
}

void mm_free(void* pointer)
{
//		struct timeval startTV,endTV,diff;
//		gettimeofday(&startTV, NULL);
		free(pointer);
//		gettimeofday(&endTV, NULL);
//		timersub(&endTV, &startTV, &diff);
//		timeradd(&diff, &mm_free_time, &mm_free_time);
		mm_count_free++;
		return;
}

void* rsalloc(size_t size){ return malloc(size); }

void* mm_std_malloc(size_t size){ return malloc(size); }

void mm_std_free(void* pointer){ free(pointer); }

void rsfree(void* pointer){ free(pointer); }
