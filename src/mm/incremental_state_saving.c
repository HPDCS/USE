#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>
#include <signal.h>

#include <ROOT-Sim.h>

#include <mm.h>
#include <dymelor.h>
#include <incremental_state_saving.h>
#include <segment.h>


extern void **mem_areas; /// pointers to the lp segment

lp_iss_metadata *iss_states; /// runtime iss metadata for each lp
model_t iss_costs_model;	 /// runtime tuning of the cost model 


static inline void guard_memory(void* pg_ptr, size_t size){
	if(!pdes_config.iss_enabled_mprotection) return;
	assert(pg_ptr >= mem_areas[current_lp] && pg_ptr < mem_areas[current_lp]+PER_LP_PREALLOCATED_MEMORY);
	assert(size <= PER_LP_PREALLOCATED_MEMORY);
	mprotect(pg_ptr, size, PROT_READ);
}

static inline void unguard_memory(void* pg_ptr, size_t size){
	if(!pdes_config.iss_enabled_mprotection) return;
	assert(pg_ptr >= mem_areas[current_lp] && pg_ptr < mem_areas[current_lp]+PER_LP_PREALLOCATED_MEMORY);
	assert(size <= PER_LP_PREALLOCATED_MEMORY);
	mprotect(pg_ptr, size, PROT_READ | PROT_WRITE);
}


bool is_next_ckpt_incremental(void) {

	if (pdes_config.checkpointing == PERIODIC_STATE_SAVING)  
		return false;

	if (iss_states[current_lp].iss_counter++ == pdes_config.ckpt_forced_full_period) {
		/// its time to take a full snapshot
		iss_states[current_lp].iss_counter = 0; 
		return false;
	}

	return true;

}


void sigsev_tracer_for_dirty(int a, siginfo_t *b, void *c){
	assert(a==SIGSEGV);
	(void)c;
	dirty(b->si_addr, 1);
}


void init_incremental_checkpoint_support(unsigned int num_lps){

	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*num_lps);
	iss_costs_model.mprotect_cost_per_page = 1;
	iss_costs_model.log_cost_per_page = 100;

	struct sigaction action;
	action.sa_sigaction = sigsev_tracer_for_dirty;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &action, NULL);

}

void init_incremental_checkpoint_support_per_lp(unsigned int lp){
	bzero(iss_states+lp, sizeof(lp_iss_metadata));
}

int get_page_idx_from_ptr(unsigned int cur_lp, void *addr){
	unsigned long long base_addr = (unsigned long long)(mem_areas[cur_lp]);
	unsigned long long pg_addr = ((unsigned long long)addr) & (~ (PAGE_SIZE-1));
	assert(pg_addr >= base_addr);
	assert(pg_addr <  (base_addr+PER_LP_PREALLOCATED_MEMORY));
	unsigned long long offset = pg_addr - base_addr;
	assert(offset < PER_LP_PREALLOCATED_MEMORY);
	return (unsigned int) offset/PAGE_SIZE + PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
}

void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id){
	assert(id>=PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE);
	assert(id<PER_LP_PREALLOCATED_MEMORY*2/PAGE_SIZE);
	return ((char*)mem_areas[cur_lp]) + (id-PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*PAGE_SIZE; 
}

unsigned int get_lowest_page_from_partition_id(unsigned int page_id){
	do{
		page_id<<=1;
	}while(page_id  < (PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*2);
	return page_id >>=1;
}

double estimate_cost(size_t size, double probability){
	return iss_costs_model.mprotect_cost_per_page + probability*iss_costs_model.log_cost_per_page*size;
}

partition_log* log_incremental(unsigned int cur_lp, simtime_t ts){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int cur_partition_size, tgt_partition_size, cur_id, tgt_id, last_dirty = 0;
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
	partition_log *cur_log = NULL, *prev_log = NULL;
	//printf("should log %llu\n", iss_states[cur_lp].current_incremental_log_size);
	while(start < end){
		cur_id = start;
        last_dirty = tgt_id = 0;
		cur_partition_size = 1;
		while(cur_id > 0){
		//	if(cur_partition_size == 1 && cur_id < 266666)printf("checking %u %u %u\n", cur_id, tree[cur_id].valid, tree[cur_id].dirty);
            if(!last_dirty && tree[cur_id].dirty) last_dirty = cur_id;
			if(tree[cur_id].valid)
			  tgt_partition_size = cur_partition_size;
			if(tree[cur_id].valid && tree[cur_id].dirty){
			  tgt_id = cur_id;
			}
			cur_id >>= 1;
			cur_partition_size <<=1;
		}
		//printf("cur_par_size %u\n", tgt_partition_size);
		if(tgt_id){
		//	printf("log partition %u %u %u\n", start, tgt_id, tgt_partition_size);
			tgt_id = get_lowest_page_from_partition_id(tgt_id);
			cur_log = (partition_log*) rsalloc(sizeof(partition_log));
			cur_log->size = tgt_partition_size*PAGE_SIZE;
			cur_log->next = prev_log;
			cur_log->ts = ts;
			cur_log->addr = get_page_ptr_from_idx(cur_lp, tgt_id);
			cur_log->log = rsalloc(tgt_partition_size*PAGE_SIZE);
			prev_log = cur_log; 

			iss_states[cur_lp].current_incremental_log_size-=cur_log->size;
			memcpy(cur_log->log, cur_log->addr, cur_log->size);
			assert(start == tgt_id);
		}
        else{
            if(last_dirty == 0) break;
            start = last_dirty <<= 1 + 1;
            start = get_lowest_page_from_partition_id(start);
        }
        start+=tgt_partition_size;

	}
	assert(iss_states[cur_lp].current_incremental_log_size == 0);
	return prev_log;
}


void log_incremental_destroy_chain(partition_log *cur){
	partition_log *next = NULL;
	while(cur){
		next = cur->next;
		rsfree(cur->log);
		rsfree(cur);
		cur = next;
	}	
}

void log_incremental_restore(partition_log *cur){
	while(cur){
		memcpy(cur->addr, cur->log, cur->size);
		cur = cur->next;
	}
}


/* Marks each partition containing the touched page as dirty */
void dirty(void* addr, size_t size){
	//printf("%u: lp %u %p\n", tid, current_lp, addr);
	unsigned int page_id    	= get_page_idx_from_ptr(current_lp, addr);
	unsigned int cur_id 		= page_id;
	partition_node_tree_t *tree = &iss_states[current_lp].partition_tree[0];

	unsigned int tgt_partition_size = 0;
	unsigned int cur_partition_size = 1;
	unsigned int partition_id = page_id;
    bool was_dirty = 0;
    
	iss_states[current_lp].total_access_count++;


	while(cur_id > 0){
        was_dirty = tree[cur_id].dirty;
		tree[cur_id].dirty = 1;
			
		if(tree[cur_id].valid){
			tgt_partition_size = cur_partition_size;
			partition_id = cur_id;
		}
        
        if(!was_dirty){
            tree[cur_id].access_count += 1;
        }
        
		cur_partition_size<<=1;
		cur_id>>=1;
	}

	partition_id = get_lowest_page_from_partition_id(partition_id);

	iss_states[current_lp].current_incremental_log_size += tgt_partition_size*PAGE_SIZE;
	unguard_memory(get_page_ptr_from_idx(current_lp, partition_id), tgt_partition_size*PAGE_SIZE);
}

void iss_unprotect_all_memory(unsigned int cur_lp){
	unguard_memory(get_page_ptr_from_idx(cur_lp, get_lowest_page_from_partition_id(1)), PER_LP_PREALLOCATED_MEMORY);
}

void iss_protect_all_memory(unsigned int cur_lp){
	guard_memory(get_page_ptr_from_idx(cur_lp,  get_lowest_page_from_partition_id(1)), PER_LP_PREALLOCATED_MEMORY);
}


void iss_first_run_model(unsigned int cur_lp){
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;

	for(unsigned int i = 0; i<start; i++){
		tree[i].valid = 0;
		tree[i].cost  = 0;
		tree[i].dirty = 0;
	}

	for(unsigned int i = start; i<end; i++){
		tree[i].valid = 1;
		tree[i].dirty = 0;
		tree[i].cost  = 0;
	} 

}


void iss_update_model(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int size  = 1;
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 

	for(unsigned int i = 0; i<start; i++){
		tree[i].valid = 0;
		tree[i].dirty = 0;
	}

	for(unsigned int i = start; i<end; i++){
		tree[i].valid = 1;
		tree[i].dirty = 0;
		tree[i].cost = estimate_cost(size, ((double)tree[i].access_count) / ((double)tree[1].access_count) );
	} 
	
	while(start > 1){
		size *= 2;
		for(unsigned int i = start; i<end;i+=2){
			unsigned int parent = i/2;
			tree[parent].cost = estimate_cost(size, ((double)tree[parent].access_count)/((double)tree[1].access_count));
			if(tree[parent].cost < tree[i].cost+tree[i+1].cost){
				tree[parent].valid = 1;
			}
			else{
				tree[parent].valid = 0;
				tree[parent].cost = tree[i].cost+tree[i+1].cost;
			}
			
		}
		end    = start;
		start /= 2; 
	}	
}


/*void iss_unprotect_memory(unsigned int cur_lp){
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int cur_id, tgt_id;
	unsigned int cur_partition_size, tgt_partition_size;

	while(start < end){
		cur_id = start;
		cur_partition_size = 1;
		while(cur_id > 0){
			if(tree[cur_id].valid){
				tgt_id = cur_id;
				tgt_partition_size = cur_partition_size;
			} 
			cur_id >>= 1;
			cur_partition_size <<=1;
		}
		tgt_id = get_lowest_page_from_partition_id(tgt_id);
		unguard_memory(get_page_ptr_from_idx(cur_lp, tgt_id), tgt_partition_size*PAGE_SIZE);
		start += tgt_partition_size;
	}

}

void iss_protect_memory(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int cur_id = 0;
	unsigned int tgt_id = 0;
	unsigned int cur_partition_size, tgt_partition_size;
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 

	
	start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	end   = start*2;
	while(start < end){
		cur_id = start;
		cur_partition_size = 1;
		while(cur_id > 0){
			if(tree[cur_id].valid){
				tgt_id = cur_id;
				tgt_partition_size = cur_partition_size;
			}
			cur_id >>= 1;
			cur_partition_size <<=1;
		}
		
		tgt_id = get_lowest_page_from_partition_id(tgt_id);
		fflush(stdout);
		guard_memory(get_page_ptr_from_idx(cur_lp, tgt_id), tgt_partition_size*PAGE_SIZE);
		start += tgt_partition_size;
	}
}
*/
