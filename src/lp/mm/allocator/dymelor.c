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
* @file dymelor.c
* @brief LP's memory pre-allocator. This layer stands below DyMeLoR, and is the
* 		connection point to the Linux Kernel Module for Memory Management, when
* 		activated.
* @author Alessandro Pellegrini
*/

#include "dymelor.h"

#include <lp/mm/segment/segment.h>
#include <lp/mm/segment/buddy.h>

#include <lpm_alloc.h>
#include <lps_alloc.h>

/// Maximum number of malloc_areas allocated during the simulation. This variable is used
/// for correctly restoring an LP's state whenever some areas are deallocated during the simulation.
//int max_num_areas;

void recoverable_init(void) {
	
	register unsigned int i;
	
	recoverable_state = glo_alloc(sizeof(malloc_state *) * pdes_config.nprocesses);

	for(i = 0; i < pdes_config.nprocesses; i++){

		recoverable_state[i] = glo_alloc(sizeof(malloc_state));
		if(recoverable_state[i] == NULL)
			rootsim_error(true, "Unable to allocate memory on malloc init");

		malloc_state_init(true, recoverable_state[i]);
	}
}

/* TODO  finalization*/


/**
* This function inizializes the dymelor subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
*/
void dymelor_init() {
	allocator_init();
	recoverable_init();
}

extern struct _buddy **buddies;
extern void **mem_areas;

/// Recoverable memory state for LPs
malloc_state **recoverable_state;
/**
* This function finalizes the dymelor subsystem
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
*/
void dymelor_fini(void){
	//recoverable_fini();
	//unrecoverable_fini();
	//allocator_fini();
}


/**
* This function inizializes a malloc_area
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param m_area The pointer to the malloc_area to initialize
* @param size The chunks' size of the malloc_area
* @param num_chunks The number of chunk of the new malloc_area
*/
static void malloc_area_init(bool recoverable, malloc_area *m_area, size_t size, int num_chunks){

	int bitmap_blocks;

	m_area->is_recoverable = recoverable;
	m_area->chunk_size = size;
	m_area->alloc_chunks = 0;
	m_area->dirty_chunks = 0;
	m_area->next_chunk = 0;
	m_area->num_chunks = num_chunks;
	m_area->state_changed = 0;
	m_area->last_access = -1;
	m_area->use_bitmap = NULL;
	m_area->dirty_bitmap = NULL;
	m_area->self_pointer = NULL;
	m_area->area = NULL;

	bitmap_blocks = num_chunks / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;

	m_area->prev = -1;
	m_area->next = -1;

	SET_AREA_LOCK_BIT(m_area);

}


/**
* This function inizializes a malloc_state
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param state The pointer to the malloc_state to initialize
*/
void malloc_state_init(bool recoverable, malloc_state *state) {

	int i, num_chunks;
	size_t chunk_size;

	state->total_log_size = 0;
	state->total_inc_size = 0;
	state->busy_areas = 0;
	state->dirty_areas = 0;
	state->num_areas = NUM_AREAS;
	state->max_num_areas = MAX_NUM_AREAS;
	state->bitmap_size = 0;
	state->dirty_bitmap_size = 0;
	state->timestamp = -1;
	state->is_incremental = false;

	state->areas = (malloc_area*)lpm_alloc(state->max_num_areas * sizeof(malloc_area));
	if(state->areas == NULL)
		rootsim_error(true, "Unable to allocate memory at %s:%d\n", __FILE__, __LINE__);

	chunk_size = MIN_CHUNK_SIZE;
	num_chunks = MIN_NUM_CHUNKS;

	for(i = 0; i < NUM_AREAS; i++){

		malloc_area_init(recoverable, &state->areas[i], chunk_size, num_chunks);
		state->areas[i].idx = i;
		chunk_size = chunk_size << 1;
		if(num_chunks > MIN_NUM_CHUNKS) {
			num_chunks = num_chunks >> 1;
		}
	}

}






/**
* This function corrects a size value. It changes the given value to the immediately greater power of two
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param size The size to correct
* @return The new size
*/
static size_t compute_size(size_t size){
	// Account for the space needed to keep the pointer to the malloc area
	size += sizeof(long long);
	size_t size_new;

	size_new = POWEROF2(size);
	if(size_new < MIN_CHUNK_SIZE)
		size_new = MIN_CHUNK_SIZE;

	return size_new;
}




/**
* Find the smallest idx associated with a free chunk in a given malloc_area.
* This function tries to keep the chunks clustered in the beginning of the malloc_area,
* so to enchance locality
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
*
* @param m_area The malloc_area to scan in order to find the next available chunk
*/
static void find_next_free(malloc_area *m_area){
	m_area->next_chunk++;

	while(m_area->next_chunk < m_area->num_chunks){

		if(!CHECK_USE_BIT(m_area, m_area->next_chunk))
			break;

		m_area->next_chunk++;
	}

}




void *do_malloc(unsigned int lid, malloc_state *mem_pool, size_t size, unsigned int numa_node) {
	malloc_area *m_area, *prev_area;
	void *ptr;
	int bitmap_blocks, num_chunks;
	size_t area_size;
	bool is_recoverable;
	int j;
	if(pdes_config.serial) {
		return lps_alloc(size);
	}
	size = compute_size(size);

	if(size > MAX_CHUNK_SIZE){
		rootsim_error(false, "Requested a memory allocation of %d but the limit is %d. Reconfigure MAX_CHUNK_SIZE. Returning NULL.\n", size, MAX_CHUNK_SIZE);
		return NULL;
	}

	j = (int)log2(size) - (int)log2(MIN_CHUNK_SIZE);
	m_area = &mem_pool->areas[j];
	is_recoverable = m_area->is_recoverable;

	while(m_area != NULL && m_area->alloc_chunks == m_area->num_chunks){
		prev_area = m_area;
		if (m_area->next == -1)
			m_area = NULL;
		else
			m_area = &(mem_pool->areas[m_area->next]);
	}

	if(m_area == NULL){

		if(mem_pool->num_areas == mem_pool->max_num_areas){

			malloc_area *tmp = NULL;

			if ((mem_pool->max_num_areas << 1) > MAX_LIMIT_NUM_AREAS)
				return NULL;

			mem_pool->max_num_areas = mem_pool->max_num_areas << 1;

			rootsim_error(true, "To reimplement\n");
//			tmp = (malloc_area *)pool_realloc_memory(mem_pool->areas, mem_pool->max_num_areas * sizeof(malloc_area));
			if(tmp == NULL){

				/**
				* @todo can we find a better way to handle the realloc failure?
				*/
				rootsim_error(false,  "DyMeLoR: cannot reallocate the block of malloc_area.\n");

				mem_pool->max_num_areas = mem_pool->max_num_areas >> 1;

				return NULL;
			}

			mem_pool->areas = tmp;
		}

		// Allocate a new malloc area
		m_area = &mem_pool->areas[mem_pool->num_areas];

		// The malloc area to be instantiated has twice the number of chunks wrt the last full malloc area for the same chunks size
		malloc_area_init(is_recoverable, m_area, size, prev_area->num_chunks << 1);

		m_area->idx = mem_pool->num_areas;
		mem_pool->num_areas++;
		prev_area->next = m_area->idx;
		m_area->prev = prev_area->idx;

	}

	if(m_area->area == NULL) {

		num_chunks = m_area->num_chunks;
		bitmap_blocks = num_chunks / NUM_CHUNKS_PER_BLOCK;
		if(bitmap_blocks < 1)
			bitmap_blocks = 1;

		area_size = sizeof(malloc_area *) + bitmap_blocks * BLOCK_SIZE * 2 + num_chunks * size;

		if(pdes_config.enable_custom_alloc)
			m_area->self_pointer = (malloc_area *)pool_get_memory(lid, area_size, numa_node);
		else{
			m_area->self_pointer = lps_alloc(area_size);
			bzero(m_area->self_pointer, area_size);
		}

		if(m_area->self_pointer == NULL){
			printf("Is recoverable: ");
			printf(is_recoverable ? "true\n" : "false\n");
			rootsim_error(true, "DyMeLoR: error allocating space\n");
		}

		m_area->dirty_chunks = 0;
		*(unsigned long long *)(m_area->self_pointer) = (unsigned long long)m_area;

		m_area->use_bitmap = (unsigned int *)((char *)m_area->self_pointer + sizeof(malloc_area *));

		m_area->dirty_bitmap = (unsigned int*)((char *)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE);

		m_area->area = (void *)((char*)m_area->use_bitmap + bitmap_blocks * BLOCK_SIZE * 2);
	}

	if(m_area->area == NULL) {
		rootsim_error(true, "Error while allocating memory at %s:%d\n", __FILE__, __LINE__);
	}

	// TODO: check how next_chunk is initialized 
	ptr = (void*)((char*)m_area->area + (m_area->next_chunk * size));

	SET_USE_BIT(m_area, m_area->next_chunk);

	bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
	if(bitmap_blocks < 1)
		bitmap_blocks = 1;

	if(m_area->is_recoverable) {
		if(m_area->alloc_chunks == 0){
			mem_pool->bitmap_size += bitmap_blocks * BLOCK_SIZE;
			mem_pool->busy_areas++;
		}

		if(m_area->state_changed == 0) {
			mem_pool->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
			mem_pool->dirty_areas++;
		}

		m_area->state_changed = 1;
		m_area->last_access = current_lvt;

		if(!CHECK_LOG_MODE_BIT(m_area)){
			if((double)m_area->alloc_chunks / (double)m_area->num_chunks > MAX_LOG_THRESHOLD){
				SET_LOG_MODE_BIT(m_area);
				mem_pool->total_log_size += (m_area->num_chunks - (m_area->alloc_chunks - 1)) * size;
			} else
				mem_pool->total_log_size += size;
		} 

		//~ int chk_size = m_area->chunk_size;
		//~ RESET_BIT_AT(chk_size, 0);
		//~ RESET_BIT_AT(chk_size, 1);
	}

	m_area->alloc_chunks++;
	find_next_free(m_area);

	// TODO: togliere
	memset(ptr, 0xe8, size);

	// Keep track of the malloc_area which this chunk belongs to
	*(unsigned long long *)ptr = (unsigned long long)m_area->self_pointer;

	//printf("[%d] Ptr:%p \t size:%lu \t result:%p \n",lid,ptr,sizeof(long long),(void*)((char*)ptr + sizeof(long long)));


	return (void*)((char*)ptr + sizeof(long long));
}

void print_max_chunk_in_area(unsigned int lp){
    int i = 0;
    int max = 0;
    printf("MAX CHUNK ID IN LP %u\n", lp);
    for(int j=0; j< 14;j++){
        max = 0;
        if(recoverable_state[lp]->areas[j].self_pointer == NULL) continue;
        for(i=0; i < (recoverable_state[lp]->areas[j].num_chunks & (-3)) ;i++){
            if(CHECK_USE_BIT(&recoverable_state[lp]->areas[j], i) && i > max)
                    max = i;
        }
        printf("MAX CHUNK FOR LP 0 %u over %u\n", max, recoverable_state[lp]->areas[j].num_chunks & (-3));
    }
}


// TODO: multiple checks on m_area->is_recoverable. The code should be refactored
// TODO: lid non necessario qui
void do_free(unsigned int lid, malloc_state *mem_pool, void *ptr) {

	(void)lid;
	

	malloc_area * m_area;
	int idx, bitmap_blocks;
	size_t chunk_size;

	if(pdes_config.serial) {
		lps_free(ptr);
		return;
	}

	if(ptr == NULL) {
		rootsim_error(false, "Invalid pointer during free\n");
		return;
	}

	m_area = get_area(ptr);
	if(m_area == NULL){
		rootsim_error(false, "Invalid pointer during free: malloc_area NULL\n");
		return;
	}

	chunk_size = m_area->chunk_size;
	RESET_BIT_AT(chunk_size, 0);
	RESET_BIT_AT(chunk_size, 1);
	idx = (int)((char *)ptr - (char *)m_area->area) / chunk_size;

	if(!CHECK_USE_BIT(m_area, idx)) {
		fprintf(stderr, "%s:%d: double free corruption or address not malloc'd\n", __FILE__, __LINE__);
		abort();
	}
	RESET_USE_BIT(m_area, idx);

        bitmap_blocks = m_area->num_chunks / NUM_CHUNKS_PER_BLOCK;
        if(bitmap_blocks < 1)
                bitmap_blocks = 1;


	if(m_area->is_recoverable) {
		if (m_area->state_changed == 0) {
			mem_pool->dirty_bitmap_size += bitmap_blocks * BLOCK_SIZE;
			mem_pool->dirty_areas++;
		}


		if(CHECK_DIRTY_BIT(m_area, idx)){
			RESET_DIRTY_BIT(m_area, idx);
			m_area->dirty_chunks--;

			if (m_area->state_changed == 1 && m_area->dirty_chunks == 0)
				mem_pool->dirty_bitmap_size -= bitmap_blocks * BLOCK_SIZE;

			mem_pool->total_inc_size -= chunk_size;

			if (m_area->dirty_chunks < 0) {
				rootsim_error(true, "%s:%d: negative number of chunks\n", __FILE__, __LINE__);
			}
		}

		m_area->state_changed = 1;

		m_area->last_access = current_lvt;
	}

	if(idx < m_area->next_chunk)
		m_area->next_chunk = idx;

	m_area->alloc_chunks--;

	if(m_area->alloc_chunks == 0) {
		mem_pool->bitmap_size -= bitmap_blocks * BLOCK_SIZE;
		mem_pool->busy_areas--;
	}

	if(m_area->is_recoverable) {
		if(CHECK_LOG_MODE_BIT(m_area)){
			if((double)m_area->alloc_chunks / (double)m_area->num_chunks < MIN_LOG_THRESHOLD){
				RESET_LOG_MODE_BIT(m_area);
				mem_pool->total_log_size -= (m_area->num_chunks - m_area->alloc_chunks) * chunk_size;
			}
		} else
			mem_pool->total_log_size -= chunk_size;
	}
	// TODO: when do we free unrecoverable areas?
}



__thread void* freezed_state_ptr[4];
 
void alloc_memory_for_freezed_state(void){
    int i=0;
    freezed_state_ptr[i++]   = lps_alloc(                   PER_LP_PREALLOCATED_MEMORY);
    freezed_state_ptr[i++]   = lpm_alloc(                   sizeof(malloc_state));
    freezed_state_ptr[i++]   = lpm_alloc(                   MAX_NUM_AREAS*sizeof(malloc_area));
    freezed_state_ptr[i++]   = lpm_alloc(                   sizeof(struct _buddy));
}

void** log_freezed_state(unsigned int lp){
    printf("freezing state\n");
    int i=0;
    memcpy(freezed_state_ptr[i++], get_base_pointer(lp),         PER_LP_PREALLOCATED_MEMORY);
    memcpy(freezed_state_ptr[i++], recoverable_state+lp,         sizeof(malloc_state));
    memcpy(freezed_state_ptr[i++], recoverable_state[lp]->areas, sizeof(malloc_area));
    memcpy(freezed_state_ptr[i++], buddies + lp,                 sizeof(malloc_area));

    return freezed_state_ptr;
}


void restore_freezed_state(unsigned int lp){
    printf("restoring state\n");
    int i=0;
    memcpy(get_base_pointer(lp),         freezed_state_ptr[i++], PER_LP_PREALLOCATED_MEMORY);
    memcpy(recoverable_state + lp,       freezed_state_ptr[i++], sizeof(malloc_state));
    memcpy(recoverable_state[lp]->areas, freezed_state_ptr[i++], sizeof(malloc_area));
    memcpy(buddies + lp,                 freezed_state_ptr[i++], sizeof(malloc_area));
}