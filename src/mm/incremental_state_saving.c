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



__thread int __in_log_full = 0;

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
    assert(__in_log_full == 0);
	(void)c;
	dirty(b->si_addr, 1);
}


void init_incremental_checkpoint_support(unsigned int num_lps){

	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*num_lps);
	iss_costs_model.mprotect_cost_per_page = 30;
	iss_costs_model.log_cost_per_page = 100;

	struct sigaction action;
	action.sa_sigaction = sigsev_tracer_for_dirty;
	action.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &action, NULL);

}

void init_incremental_checkpoint_support_per_lp(unsigned int lp){
	bzero(iss_states+lp, sizeof(lp_iss_metadata));
   	iss_first_run_model(current_lp); 

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

float estimate_cost(size_t size, float probability){
    float cost = iss_costs_model.mprotect_cost_per_page + probability*iss_costs_model.log_cost_per_page*size;
    assert(cost>0);
	return cost;
}



partition_log* log_incremental(unsigned int cur_lp, simtime_t ts){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int cur_partition_size, tgt_partition_size, cur_id, tgt_id, first_dirty_size = 0, first_dirty = 0;
    unsigned int cur_start;
    unsigned int cur_end = end;
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
	partition_log *cur_log = NULL, *prev_log = NULL;
    unsigned int or_size = iss_states[cur_lp].current_incremental_log_size;
    int prev = 0;
    unsigned int last_restart = 0;
    unsigned short cur_dirty_ts = iss_states[cur_lp].cur_virtual_ts;
	   
    unsigned int tmp = 0, last_computed = 0;
    unsigned int prev_end;
    unsigned int prev_first_dirty_end = end;

	while(start < end){
		//printf("%u start %u\n", tid, start);
        assert(last_restart < start);
        last_restart = start;
        cur_id = start;
        first_dirty = tgt_id = 0;
		tgt_partition_size = cur_partition_size = 1;
        tmp = 0;
        last_computed = 0;
        prev_end = end;
        
		while(cur_id > 0){

           if(!first_dirty && (tree[cur_id].dirty == cur_dirty_ts) )  {
                first_dirty = cur_id;
                first_dirty_size = cur_partition_size;
                prev_first_dirty_end = prev_end;
           }
			
            if(tree[cur_id].valid[0] && (tree[cur_id].dirty == cur_dirty_ts) ){
			  tgt_partition_size = cur_partition_size;
			  tgt_id = cur_id;
			}
            if(!first_dirty) prev = cur_id;
			cur_id >>= 1;
			cur_partition_size <<=1;
            prev_end >>= 1;
		}
		if(tgt_id){
            
			tgt_id = get_lowest_page_from_partition_id(tgt_id);
			cur_log = (partition_log*) rsalloc(sizeof(partition_log));
			cur_log->size = tgt_partition_size*PAGE_SIZE;
			cur_log->next = prev_log;
			cur_log->ts = ts;
			cur_log->addr = get_page_ptr_from_idx(cur_lp, tgt_id);
			cur_log->log = rsalloc(cur_log->size);
			prev_log = cur_log; 

			iss_states[cur_lp].current_incremental_log_size -= cur_log->size;
			memcpy(cur_log->log, cur_log->addr, cur_log->size);
			assert(start == tgt_id);
		}
        else{
            if(first_dirty == 0) break;
            tmp = first_dirty;
            tgt_partition_size = (prev == 0);
            last_computed = first_dirty;
            if(prev & 1){
                last_computed +=  1;
                if(last_computed >= prev_first_dirty_end) break;
                last_computed = get_lowest_page_from_partition_id(last_computed);
                assert(start != last_computed);
                start = last_computed;
            }
            else if(prev !=0){
                tgt_partition_size = 0;
                first_dirty <<= 1;
                first_dirty +=  1;
                first_dirty = get_lowest_page_from_partition_id(first_dirty);
                assert(start != first_dirty);
                start = first_dirty;
            }
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
    iss_states[current_lp].count_tracked++;
	unsigned int tgt_partition_size = 0;
	unsigned int cur_partition_size = 1;
	unsigned int partition_id = page_id;
    bool was_dirty = 0;
    unsigned short cur_dirty_ts =  iss_states[current_lp].cur_virtual_ts;

	while(cur_id > 0){
        was_dirty = tree[cur_id].dirty == cur_dirty_ts;
			
		if(tree[cur_id].valid[0]){
			tgt_partition_size = cur_partition_size;
			partition_id = cur_id;
		}
        
        if(!was_dirty){
            tree[cur_id].dirty = cur_dirty_ts;
            assert(tree[cur_id].access_count>=0);
            tree[cur_id].access_count += 1;
            assert(tree[cur_id].access_count>=0);
            tree[cur_id].cost = estimate_cost(cur_partition_size, ((float)tree[cur_id].access_count) / ((float)iss_states[current_lp].iss_model_round+1) );
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
    if(cur_lp == 0) printf("NUM PAGES:%u\n", start);
    iss_states[cur_lp].cur_virtual_ts = 1;
	for(unsigned int i = 0; i<end; i++){
        tree[i].valid[0] = i>=start;
		tree[i].dirty = 0;
		tree[i].cost  = 0.0;
        tree[i].access_count = 0;
	} 
    //tree[1].valid[0] = 1; //i>=start;
		

}


void iss_log_incremental_reset(unsigned int lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
    unsigned int cur_model = iss_states[lp].current_model; 
    partition_node_tree_t *tree = &iss_states[lp].partition_tree[0]; 

    iss_states[lp].current_incremental_log_size = 0;
    iss_states[lp].count_tracked = 0;
    
    
    if(iss_states[lp].cur_virtual_ts == 65000){
        iss_states[lp].cur_virtual_ts = 0;

        for(unsigned int i = 0; i<end; i++){
            tree[i].dirty = 0;
        } 
    }
    
    iss_states[lp].cur_virtual_ts += 1;
    

}

void iss_update_model(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int size  = 1;
    unsigned int parent= 0;
    unsigned int cur_model = iss_states[cur_lp].current_model; 
	unsigned int cur_round = 0;
    partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
    iss_states[current_lp].iss_model_round++;
    if((iss_states[current_lp].iss_model_round%100)!=0) return;
    //return;
    //iss_states[cur_lp].cur_virtual_ts = 1;

    if(iss_states[cur_lp].disabled < 2){
        iss_states[cur_lp].disabled++;
        for(unsigned int i = 0; i<start; i++){
	//	tree[i].dirty = 0;
        tree[i].valid[cur_model] = 0;
	}

	for(unsigned int i = start; i<end; i++){
	//	tree[i].dirty = 0;
		tree[i].valid[cur_model] = 1;
	} 
    
    
    }
    if(iss_states[cur_lp].disabled == 2){
        int counts[20];
        int idx=0;
        if(cur_lp == 0)
            printf("model update\n");
        iss_states[cur_lp].disabled++;

        while(start > 1){
            size *= 2;
            for(unsigned int i = start; i<end;i+=2){
                parent = i/2;
                if(tree[parent].cost < tree[i].cost+tree[i+1].cost){
                                   tree[parent].valid[cur_model] = 1;
                }
                else{
                                   tree[parent].valid[cur_model] = 0;
                    tree[parent].cost = tree[i].cost+tree[i+1].cost;
                }
                
            }
            end    = start;
            start /= 2; 
        }
        
        start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
        end = start*2;
        
        if( current_lp == 0){
            start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
            end = start*2;
            int j=0;
            while(start >0){
                printf("LAYER %u\n", j);
                for(unsigned int i =start; i<end;i+=2){
                    if(tree[i].cost > 0 || tree[i+1].cost > 0 ){
                        printf("\t%u: %f %u %f\n", i, tree[i].cost, tree[i].valid[0], ((float)tree[i].access_count) / ((float)iss_states[current_lp].iss_model_round));
                        if(i != 1)
                        printf("\t%u: %f %u %f\n\n", i+1, tree[i+1].cost, tree[i+1].valid[0], ((float)tree[i+1].access_count) / ((float)iss_states[current_lp].iss_model_round));
                    }
                }
                end >>=1;
                start >>=1;
                j++;
            }
            start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
            end = start*2;

        }
               
        
    }
    

}

