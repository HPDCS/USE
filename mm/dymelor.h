/**
*			Copyright (C) 2008-2015 HPDCS Group
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
* @file dymelor.h
* @brief This is the Memory Management Subsystem main header file
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @date April 02, 2008
*
*/


#pragma once
#ifndef _DYMELOR_H
#define _DYMELOR_H

#include <math.h>
#include <string.h>

#include <ROOT-Sim.h>
#include <timer.h>
#include <core.h>



/**************************************
 * DyMeLoR definitions and structures *
 **************************************/



#define MASK 0x00000001		// Mask used to check, set and unset bits


//#define NUM_CHUNKS_PER_BLOCK 32
#define NUM_CHUNKS_PER_BLOCK 32
#define BLOCK_SIZE sizeof(unsigned int)


#define MIN_CHUNK_SIZE 64	// Size (in bytes) of the smallest chunk provideable by DyMeLoR
#define MAX_CHUNK_SIZE 2048	// Size (in bytes) of the biggest one. Notice that if this number
				// is too large, performance (and memory usage) might be affected.
				// If it is too small, large amount of memory requests by the
				// application level software (i.e, larger than this number)
				// will fail, as DyMeLoR will not be able to handle them!

#define NUM_AREAS (log2(MAX_CHUNK_SIZE) - log2(MIN_CHUNK_SIZE) + 1)			// Number of initial malloc_areas available (will be increased at runtime if needed)
#define MAX_NUM_AREAS (NUM_AREAS * 4) 	// Maximum number of allocatable malloc_areas. If MAX_NUM_AREAS
				// malloc_areas are filled at runtime, subsequent malloc() requests
				// by the application level software will fail.
#define MAX_LIMIT_NUM_AREAS 100
#define MIN_NUM_CHUNKS 2*4096	// Minimum number of chunks per malloc_area
#define MAX_NUM_CHUNKS 4*4096	// Maximum number of chunks per malloc_area

#define MAX_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is underused TODO: retest
#define MIN_LOG_THRESHOLD 1.7	// Threshold to check if a malloc_area is overused TODO: retest


#ifndef INCREMENTAL_GRANULARITY
 #define INCREMENTAL_GRANULARITY 50 // Number of incremental logs before a full log is forced
#endif

// These macros are used to tune the statistical malloc_area diff
#define LITTLE_SIZE		32
#define CHECK_SIZE		0.25  // Must be <= 0.25!


// This macro is used to retrieve a cache line in O(1)
#define GET_CACHE_LINE_NUMBER(P) ((unsigned long)((P >> 4) & (CACHE_SIZE - 1)))

// Macros to check, set and unset bits in the malloc_area masks
#define CHECK_USE_BIT(A,I) ( CHECK_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define SET_USE_BIT(A,I) ( SET_BIT_AT(										\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define RESET_USE_BIT(A,I) ( RESET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->use_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],	\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )

#define CHECK_DIRTY_BIT(A,I) ( CHECK_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define SET_DIRTY_BIT(A,I) ( SET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )
#define RESET_DIRTY_BIT(A,I) ( RESET_BIT_AT(									\
			((unsigned int*)(((malloc_area*)A)->dirty_bitmap))[(int)((int)I / NUM_CHUNKS_PER_BLOCK)],\
			((int)I % NUM_CHUNKS_PER_BLOCK)) )


// Macros uset to check, set and unset special purpose bits
#define SET_LOG_MODE_BIT(A)     ( SET_BIT_AT(((malloc_area*)A)->chunk_size, 0) )
#define RESET_LOG_MODE_BIT(A) ( RESET_BIT_AT(((malloc_area*)A)->chunk_size, 0) )
#define CHECK_LOG_MODE_BIT(A) ( CHECK_BIT_AT(((malloc_area*)A)->chunk_size, 0) )

#define SET_AREA_LOCK_BIT(A)     ( SET_BIT_AT(((malloc_area*)A)->chunk_size, 1) )
#define RESET_AREA_LOCK_BIT(A) ( RESET_BIT_AT(((malloc_area*)A)->chunk_size, 1) )
#define CHECK_AREA_LOCK_BIT(A) ( CHECK_BIT_AT(((malloc_area*)A)->chunk_size, 1) )

#define SET_BIT_AT(B,K) ( B |= (MASK << K) )
#define RESET_BIT_AT(B,K) ( B &= ~(MASK << K) )
#define CHECK_BIT_AT(B,K) ( B & (MASK << K) )


/// This structure let DyMeLoR handle one malloc area (for serving given-size memory requests)
struct _malloc_area {
	bool is_recoverable;
	size_t chunk_size;
	int alloc_chunks;
	int dirty_chunks;
	int next_chunk;
	int num_chunks;
	int idx;
	int state_changed;
	simtime_t last_access;
	struct _malloc_area *self_pointer; // This pointer is used in a free operation. Each chunk points here. If malloc_area is moved, only this is updated.
	unsigned int *use_bitmap;
	unsigned int *dirty_bitmap;
	void *area;
	int prev;
	int next;
};

typedef struct _malloc_area malloc_area;



/// Definition of the memory map
struct _malloc_state {
	bool is_incremental;		/// Tells if it is an incremental log or a full one (when used for logging)
	size_t total_log_size;
	size_t total_inc_size;
	size_t bitmap_size;
	size_t dirty_bitmap_size;
	int num_areas;
	int max_num_areas;
        int busy_areas;
	int dirty_areas;
	simtime_t timestamp;
	struct _malloc_area *areas;
};

typedef struct _malloc_state malloc_state;




#define is_incremental(ckpt) (((malloc_state *)ckpt)->is_incremental == true)

#define get_top_pointer(ptr) ((long long *)((char *)ptr - sizeof(long long)))
#define get_area_top_pointer(ptr) ( (malloc_area **)(*get_top_pointer(ptr)) )
#define get_area(ptr) ( *(get_area_top_pointer(ptr)) )



/***************
 * EXPOSED API *
 ***************/


extern malloc_state *m_state[MAX_LPs];


// DyMeLoR API
extern void dymelor_init(void);
extern void dymelor_fini(void);
extern void set_force_full(unsigned int, int);
extern void dirty_mem(void *, int);
extern size_t get_state_size(int);
extern size_t get_log_size(malloc_state *);
extern size_t get_inc_log_size(void *);
extern int get_granularity(void);
extern size_t dirty_size(unsigned int, void *, double *);
extern void recoverable_init(void);
extern void recoverable_fini(void);
extern void malloc_state_init(bool recoverable, malloc_state *state);
extern void *do_malloc(unsigned int, malloc_state * mem_pool, size_t size);
extern void do_free(unsigned int, malloc_state *mem_pool, void *ptr);
extern void *pool_get_memory(unsigned int lid, size_t size);
extern void pool_release_memory(unsigned int lid, void *ptr);

// Recoverable Memory API
extern void *__wrap_malloc(size_t);
extern void __wrap_free(void *);
extern void *__wrap_realloc(void *, size_t);
extern void *__wrap_calloc(size_t, size_t);
extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_realloc(void *, size_t);
extern void *__real_calloc(size_t, size_t);


extern malloc_state **recoverable_state;



#endif
