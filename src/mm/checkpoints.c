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
* @file checkpoints.c
* @brief This module implements the routines to save and restore checkpoints,
*        both full and incremental
* @author Roberto Toccaceli
* @author Alessandro Pellegrini
* @author Roberto Vitali
* @author Francesco Quaglia
*/
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <dymelor.h>
#include <timer.h>
#include <core.h>
//#include <scheduler/scheduler.h>
//#include <scheduler/process.h>
#include <state.h>
#include <statistics.h>
#include <autockpt.h>

#include <incremental_state_saving.h>



int compute_bitmap_blocks(int chunks) {

	int blocks = chunks / NUM_CHUNKS_PER_BLOCK;
	if (blocks < 1)
		blocks = 1;

	return blocks;

}

void set_chunk_dirty_from_address(void *addr, int lp) {

	size_t chunk_size, area_size;
	int j, k, i, blocks, num_chunks;
	unsigned long long marea_base_addr, chunk_base_address;
	malloc_area *target_marea;
	malloc_state *m_state;
	unsigned long long offset;
	unsigned int index;

	unsigned long long start_address = (unsigned long long)addr;

	m_state = recoverable_state[lp];
	int num_areas = m_state->num_areas;

	malloc_area *areas = m_state->areas;
	assert(areas != NULL);
	

	for (j=0; j < num_areas; j++) {
		num_chunks = areas[j].num_chunks;
		chunk_size = areas[j].chunk_size;
		RESET_BIT_AT(chunk_size, 0);	// ckpt Mode bit
		RESET_BIT_AT(chunk_size, 1);	// Lock bit
		blocks = compute_bitmap_blocks(num_chunks);
		area_size = num_chunks * chunk_size;
		marea_base_addr = ((unsigned long long) areas[j].area); 
		if (start_address >= marea_base_addr && start_address < marea_base_addr + area_size) {
			target_marea = &areas[j];
			offset = start_address - marea_base_addr;
			index = offset / chunk_size;
			SET_DIRTY_BIT(target_marea, index); 
			target_marea->dirty_chunks++;
			break;
		}
	}

	
}

void clean_bitmap(int blocks, unsigned int *bitmap) {

	int j;
	if (bitmap != NULL) {
		for(j = 0; j < blocks; j++)
			bitmap[j] = 0;
		
	}
}


bool check_not_used_chunk_and_clean(malloc_area *m_area, int blocks) {


	// Check if there is at least one chunk used in the area
	if(m_area->alloc_chunks == 0 && m_area->dirty_chunks == 0){ /// skip areas not dirtied and not used

		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;

		clean_bitmap(m_area->use_bitmap, blocks, &(m_area->dirty_bitmap));
		
		return true; /// do not log this area skip to the next one
	}

	return false; /// take a snapshot (full or incremental)
}



void copy_chunks_in_marea(malloc_area *m_area, unsigned int *bitmap, int blocks, void **ptr, size_t chunk_size) {

	int j, k, idx;
	// Copy only the in-use chunks (allocated or dirtied)
	for(j = 0; j < blocks; j++){

		// Check the allocation bitmap on a per-block basis, to enhance scan speed
		if(bitmap[j] == 0) {
			// Empty (no-chunks-allocated) block: skip to the next
			continue;

		} else {
			// At least one chunk is in-use: per-bit scan of the block is required
			for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){

				if(CHECK_BIT_AT(bitmap[j], k)){

					idx = j * NUM_CHUNKS_PER_BLOCK + k;
					memcpy(*ptr, (void*)((char*)m_area->area + (idx * chunk_size)), chunk_size);
					*ptr = (void*)((char*) *ptr + chunk_size);

				}
			}
		}
	}
}


void log_all_marea_chunks(malloc_area *m_area, void **ptr, int bitmap_blocks, int dirty_blocks, bool is_incremental) {

	size_t chunk_size;

	chunk_size = m_area->chunk_size;
	RESET_BIT_AT(chunk_size, 0);	// ckpt Mode bit
	RESET_BIT_AT(chunk_size, 1);	// Lock bit

	// Check whether the area should be completely copied (not on a per-chunk basis)
	// using a threshold-based heuristic
	if(!is_incremental && CHECK_LOG_MODE_BIT(m_area)){

		// If the malloc_area is almost (over a threshold) full, copy it entirely
		memcpy(*ptr, m_area->area, m_area->num_chunks * chunk_size);
		*ptr = (void*)((char*) *ptr + m_area->num_chunks * chunk_size);

	} else {
		

		if (is_incremental) {
			memcpy(*ptr, m_area->dirty_bitmap, bitmap_blocks * BLOCK_SIZE);
			*ptr = (void*)((char*) *ptr + bitmap_blocks * BLOCK_SIZE);
			copy_chunks_in_marea(m_area, m_area->dirty_bitmap, bitmap_blocks, ptr, chunk_size);
		} else {
			copy_chunks_in_marea(m_area, m_area->use_bitmap, bitmap_blocks, ptr, chunk_size);
		}

	}
}




/**
* This function creates a full log of the current simulation states and returns a pointer to it.
* The algorithm behind this function is based on packing of the really allocated memory chunks into
* a contiguous memory area, exploiting some threshold-based approach to fasten it up even more.
* The contiguous copy is performed precomputing the size of the full log, and then scanning it
* using a pointer for storing the relevant information.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008
*
* To fully understand the changes in this function to support the incremental logging as well,
* please point to the paper:
* 	A. Pellegrini, R. Vitali, F. Quaglia
* 	Di-DyMeLoR: Logging only Dirty Chunks for Efficient Management of Dynamic Memory Based
* 	Optimistic Simulation Objects
*	Proceedings of the 23rd Workshop on Principles of Advanced and Distributed Simulation
*	2009
*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @return A pointer to a malloc()'d memory area which contains the full log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*
* @todo must be declared static. This will entail changing the logic in gvt.c to save a state before rebuilding.
*/
void *log_full(int lid) {

	void *ptr = NULL, *ckpt = NULL;
	int i, bitmap_blocks, dirty_blocks;
	size_t size;
	malloc_area *m_area;
	partition_log *partial_log;

	// Timers for self-tuning of the simulation platform
	clock_timer checkpoint_timer;
	clock_timer_start(checkpoint_timer);


	recoverable_state[lid]->is_incremental = is_next_ckpt_incremental(); /// call routine for determining the type of checkpointing
	size = get_log_size(recoverable_state[lid]);

	ckpt = rsalloc(size);

	if(ckpt == NULL) {
		rootsim_error(true, "(%d) Unable to acquire memory for checkpointing the current state (memory exhausted?)");
	}

	ptr = ckpt;

	// Copy malloc_state in the ckpt
	memcpy(ptr, recoverable_state[lid], sizeof(malloc_state));
	ptr = (void *)((char *)ptr + sizeof(malloc_state));
	((malloc_state*)ckpt)->timestamp = current_lvt;

	// Copy the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(ptr, &LPS[lid]->seed, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));


	/// log not supported by dymelor metadata
	if(!pdes_config.mem_support_log && recoverable_state[lid]->is_incremental){
		/// partial log
		partial_log = log_incremental(lid, lvt(lid));
		*((void ** )ptr) = partial_log;
		ptr = (void *) ((char *) ptr + sizeof(void *));
	}
		

	for(i = 0; i < recoverable_state[lid]->num_areas; i++){

		m_area = &recoverable_state[lid]->areas[i];

		bitmap_blocks = compute_bitmap_blocks(m_area->num_chunks);
		dirty_blocks = compute_bitmap_blocks(m_area->dirty_chunks);
		
		if (check_not_used_chunk_and_clean(m_area, dirty_blocks)) continue;

		// Copy malloc_area into the ckpt
		memcpy(ptr, m_area, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));

		memcpy(ptr, m_area->use_bitmap, bitmap_blocks * BLOCK_SIZE);
		ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		if (pdes_config.mem_support_log) {
			// Copy dirty_bitmap into the ckpt
			
			log_all_marea_chunks(m_area, &ptr, bitmap_blocks, dirty_blocks, recoverable_state[lid]->is_incremental);
		}


		// Reset Dirty Bitmap, as there is a full ckpt in the chain now
		clean_bitmap(m_area->use_bitmap, dirty_blocks, &(m_area->dirty_bitmap));
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;
		bzero((void *)m_area->dirty_bitmap, dirty_blocks * BLOCK_SIZE); 

	} // For each m_area in recoverable_state


	
	// Sanity check
	if ((char *)ckpt + size != ptr){
		rootsim_error(true, "Actual (full) ckpt size is wrong by %d bytes!\nlid = %d ckpt = %p size = %#x (%d), ptr = %p, ckpt + size = %p\n", (char *)ckpt + size - (char *)ptr, lid, ckpt, size, size, ptr, (char *)ckpt + size);
	}

	recoverable_state[lid]->dirty_areas = 0;
	recoverable_state[lid]->dirty_bitmap_size = 0;
	recoverable_state[lid]->total_inc_size = 0;

	/// enable protection after log
	if (!pdes_config.mem_support_log && pdes_config.checkpointing == INCREMENTAL_STATE_SAVING) {
		iss_update_model(lid);
		iss_protect_all_memory(lid);
	}

	statistics_post_lp_data(lid, STAT_CKPT_TIME, (double)clock_timer_value(checkpoint_timer));
	statistics_post_lp_data(lid, STAT_CKPT_MEM, (double)size);
	autockpt_update_ema_full_log(lid, (double)clock_timer_value(checkpoint_timer));
	return ckpt;
}



/**
* This function is the only log function which should be called from the simulation platform. Actually,
* it is a demultiplexer which calls the correct function depending on the current configuration of the
* platform. Note that this function only returns a pointer to a malloc'd area which contains the
* state buffers. This means that this memory area cannot be used as-is, but should be wrapped
* into a state_t structure, which gives information about the simulation state pointer (defined
* via <SetState>() by the application-level code and the lvt associated with the log.
* This is done implicitly by the <LogState>() function, which in turn connects the newly taken
* snapshot with the currencly-scheduled LP.
* Therefore, any point of the simulator which wants to take a (real) log, shouldn't call directly this
* function, rather <LogState>() should be used, after having correctly set current_lp and current_lvt.
*
* @author Alessandro Pellegrini
*
* @param lid The logical process' local identifier
* @return A pointer to a malloc()'d memory area which contains the log of the current simulation state,
*         along with the relative meta-data which can be used to perform a restore operation.
*/
void *log_state(int lid) {
	statistics_post_lp_data(lid, STAT_CKPT, 1.0);
	return log_full(lid);
}



bool check_marea_rebuild(malloc_area *m_area, void **ptr, int bitmap_blocks, int dirty_blocks, int *restored_areas, malloc_state **recoverable_state) {


	if( (*restored_areas == (*recoverable_state)->busy_areas && (*recoverable_state)->dirty_areas == 0) || m_area->idx != ((malloc_area*) *ptr)->idx){

		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;

		clean_bitmap(m_area->use_bitmap, dirty_blocks, &(m_area->dirty_bitmap));
		
		m_area->alloc_chunks = 0;
		m_area->next_chunk = 0;
		RESET_LOG_MODE_BIT(m_area);
		RESET_AREA_LOCK_BIT(m_area);

		clean_bitmap(m_area->use_bitmap, bitmap_blocks, &(m_area->use_bitmap));
		clean_bitmap(m_area->use_bitmap, dirty_blocks, &(m_area->dirty_bitmap));
		
		m_area->last_access = (*recoverable_state)->timestamp;

		return true;
	}

	return false;
}


void restore_chunks_in_marea(malloc_area *m_area, unsigned int *bitmap, int blocks, void **ptr, size_t chunk_size) {

	int j, k, idx;

	for(j = 0; j < blocks; j++){

		// Check the allocation bitmap on a per-block basis, to enhance scan speed
		if(bitmap[j] == 0){
			// Empty (no-chunks-allocated) block: skip to the next
			continue;

		} else {
			// Scan the bitmap on a per-bit basis
			for(k = 0; k < NUM_CHUNKS_PER_BLOCK; k++){

				if(CHECK_BIT_AT(bitmap[j], k)){

					idx = j * NUM_CHUNKS_PER_BLOCK + k;
					memcpy((void*)((char*)m_area->area + (idx * chunk_size)), *ptr, chunk_size);
					*ptr = (void*)((char*) *ptr + chunk_size);

				}
			}
		}
	}

}


void restore_marea_chunk(malloc_area *m_area, void **ptr, int bitmap_blocks, int dirty_blocks, bool is_incremental) {

	size_t chunk_size;

	chunk_size = m_area->chunk_size;
	RESET_BIT_AT(chunk_size, 0);
	RESET_BIT_AT(chunk_size, 1);

	// Check how the area has been logged
	if(!is_incremental && CHECK_LOG_MODE_BIT(m_area)){
		// The area has been entirely logged
		memcpy(m_area->area, *ptr, m_area->num_chunks * chunk_size);
		*ptr = (void*)((char*) *ptr + m_area->num_chunks * chunk_size);

	} else {
		// The area was partially logged.
		// Logged chunks are the ones associated with a used bit whose value is 1
		// Their number is in the alloc_chunks counter
		

		if (is_incremental) {
			memcpy(m_area->dirty_bitmap, *ptr, bitmap_blocks * BLOCK_SIZE);
			*ptr = (void*)((char*) *ptr + bitmap_blocks * BLOCK_SIZE);
			restore_chunks_in_marea(m_area, m_area->dirty_bitmap, bitmap_blocks, ptr, chunk_size);
		} else {
			restore_chunks_in_marea(m_area, m_area->use_bitmap, bitmap_blocks, ptr, chunk_size);
		}
		
	}


}



/**
* This function restores a full log in the address space where the logical process will be
* able to use it as the current state.
* Operations performed by the algorithm are mostly the opposite of the corresponding log_full
* function.
*
* For further information, please see the paper:
* 	R. Toccaceli, F. Quaglia
* 	DyMeLoR: Dynamic Memory Logger and Restorer Library for Optimistic Simulation Objects
* 	with Generic Memory Layout
*	Proceedings of the 22nd Workshop on Principles of Advanced and Distributed Simulation
*	2008

*
* @author Roberto Toccaceli
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*
* @param lid The logical process' local identifier
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*/
void restore_full(int lid, void *ckpt) {

	void * ptr;
	int i, bitmap_blocks, dirty_blocks, original_num_areas, restored_areas;
	malloc_area *m_area, *new_area;

	// Timers for simulation platform self-tuning
	clock_timer recovery_timer;
	clock_timer_start(recovery_timer);
	

	restored_areas = 0;
	ptr = ckpt;
	original_num_areas = recoverable_state[lid]->num_areas;
	new_area = recoverable_state[lid]->areas;

	// Restore malloc_state
	memcpy(recoverable_state[lid], ptr, sizeof(malloc_state));
	ptr = (void*)((char*)ptr + sizeof(malloc_state));

	// Restore the per-LP Seed State (to make the numerical library rollbackable and PWD)
	memcpy(&LPS[lid]->seed, ptr, sizeof(seed_type));
	ptr = (void *)((char *)ptr + sizeof(seed_type));

	if(!pdes_config.mem_support_log && recoverable_state[lid]->is_incremental){
		log_incremental_restore(*(partition_log **)ptr);
		ptr = (void *)((char *)ptr + sizeof(void*));
	}


	recoverable_state[lid]->areas = new_area;

	// Scan areas and chunk to restore them
	for(i = 0; i < recoverable_state[lid]->num_areas; i++){

		m_area = &recoverable_state[lid]->areas[i];

		bitmap_blocks = compute_bitmap_blocks(m_area->num_chunks);
		dirty_blocks = compute_bitmap_blocks(m_area->dirty_chunks);
		if (check_marea_rebuild(m_area, &ptr, bitmap_blocks, dirty_blocks, &restored_areas, &recoverable_state[lid])) continue;


		// Restore the malloc_area
		memcpy(m_area, ptr, sizeof(malloc_area));
		ptr = (void*)((char*)ptr + sizeof(malloc_area));


		restored_areas++;

		// Restore use bitmap
		memcpy(m_area->use_bitmap, ptr, bitmap_blocks * BLOCK_SIZE);
		ptr = (void*)((char*)ptr + bitmap_blocks * BLOCK_SIZE);

		if (pdes_config.mem_support_log) {
			// Restore dirty_bitmap
			restore_marea_chunk(m_area, &ptr, bitmap_blocks, dirty_blocks, recoverable_state[lid]->is_incremental);
		}

		// Reset dirty bitmap
		bzero(m_area->dirty_bitmap, dirty_blocks * BLOCK_SIZE);
		m_area->dirty_chunks = 0;
		m_area->state_changed = 0;

		
	}


	// Check whether there are more allocated areas which are not present in the log
	if(original_num_areas > recoverable_state[lid]->num_areas){

		for(i = recoverable_state[lid]->num_areas; i < original_num_areas; i++){

			m_area = &recoverable_state[lid]->areas[i];
			m_area->alloc_chunks = 0;
			m_area->dirty_chunks = 0;
			m_area->state_changed = 0;
			m_area->next_chunk = 0;
			m_area->last_access = recoverable_state[lid]->timestamp;
			recoverable_state[lid]->areas[m_area->prev].next = m_area->idx;

			RESET_LOG_MODE_BIT(m_area);
			RESET_AREA_LOCK_BIT(m_area);

			if (m_area->use_bitmap != NULL) {

				bitmap_blocks = compute_bitmap_blocks(m_area->num_chunks);

				clean_bitmap(m_area->use_bitmap, bitmap_blocks, &(m_area->use_bitmap));
				clean_bitmap(m_area->use_bitmap, dirty_blocks, &(m_area->dirty_bitmap));

			}
		}
		recoverable_state[lid]->num_areas = original_num_areas;
	}

	recoverable_state[lid]->timestamp = -1;
	recoverable_state[lid]->is_incremental = false;
	recoverable_state[lid]->dirty_areas = 0;	
	recoverable_state[lid]->dirty_bitmap_size = 0;
	recoverable_state[lid]->total_inc_size = 0;

	/// enable protection after restore
	if (!pdes_config.mem_support_log && pdes_config.checkpointing == INCREMENTAL_STATE_SAVING) {
		iss_update_model(lid);
		iss_protect_all_memory(lid);
	}
	
	statistics_post_lp_data(lid, STAT_RECOVERY_TIME, (double)clock_timer_value(recovery_timer));
}



/**
* Upon the decision of performing a rollback operation, this function is invoked by the simulation
* kernel to perform a restore operation.
* This function checks the mark in the malloc_state telling whether we're dealing with a full or
* partial log, and calls the proper function accordingly
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param lid The logical process' local identifier
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*/
void log_restore(int lid, state_t *state_queue_node) {
	statistics_post_lp_data(lid, STAT_RECOVERY, 1.0);
	
	if(pdes_config.checkpointing == INCREMENTAL_STATE_SAVING){
		iss_unprotect_all_memory(lid);
		restore_full(lid, state_queue_node->log);
		iss_protect_all_memory(lid);
	}
	else
		restore_full(lid, state_queue_node->log);
	
}



/**
* This function is called directly from the simulation platform kernel to delete a certain log
* during the fossil collection.
*
* @author Alessandro Pellegrini
* @author Roberto Vitali
*
* @param queue_node a pointer to the simulation state which must be restored in the logical process
*
*/
void log_delete(void *ckpt){
	void *tmp;
	if(ckpt != NULL) {
		if(((malloc_state*)ckpt)->is_incremental){
				tmp = (void*)((char*)ckpt + sizeof(malloc_state));
				tmp = (void *)((char *)tmp + sizeof(seed_type));
				log_incremental_destroy_chain(*(partition_log**)tmp);
		}
		rsfree(ckpt);
	}
}

