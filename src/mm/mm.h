/**
*			Copyright (C) 2008-2017 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file allocator.h
* @brief
* @author Francesco Quaglia
*/

#pragma once

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include <core.h>
#include <atomic.h>

struct _buddy {
	size_t size;
	size_t longest[1];
};


//#ifdef HAVE_NUMA
extern void **mem_areas;
//#endif

// TODO: no need to keep a structure anymore...
// This is for the segment allocator
typedef struct _lp_mem_region{
	char* base_pointer;
}lp_mem_region;

#define SUCCESS_AECS                  0
#define FAILURE_AECS                 -1
#define INVALID_SOBJS_COUNT_AECS     -99
#define INIT_ERROR_AECS              -98
#define INVALID_SOBJ_ID_AECS         -97
#define MDT_RELEASE_FAILURE_AECS     -96

void *get_base_pointer(unsigned int gid);

extern void *get_segment(unsigned int i, unsigned int numa_node, void ***pages);
extern void segment_init(void);
extern void migrate_segment(unsigned int id, int numa_node);
extern  void ***pages;
