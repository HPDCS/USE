#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

#include <ROOT-Sim.h>

#include <mm.h>
#include <dymelor.h>
#include <segment.h>
#include <incremental_state_saving.h>


tracking_data **t_data;
int device_fd;

extern void **mem_areas; /// pointers to the lp segment


__thread int __in_log_full = 0;


/** syscalls */
int track_memory(unsigned long address, size_t size){
	return syscall(PROTECT_MEM,address, size);
}

int untrack_memory(unsigned long address, size_t size){
	return syscall(UNPROTECT_MEM,address, size);
}

int flush_local_tlb(unsigned long address, size_t size){
	return syscall(FLUSH_LOCAL_TLB,address, size);
}

/** syscalls' wrappers */

int guard_memory(unsigned int lid, unsigned long size) {

	assert(size <= PER_LP_PREALLOCATED_MEMORY);

	if(!pdes_config.iss_enabled_mprotection) {
		unsigned int id = get_lowest_page_from_partition_id(1);
		return mprotect(get_page_ptr_from_idx(lid, id ), size, PROT_READ);
	}
	else
		return track_memory((unsigned long) mem_areas[lid], size);
}

int unguard_memory(unsigned int lid, unsigned long size) {
	
	assert(size <= PER_LP_PREALLOCATED_MEMORY);

	if(!pdes_config.iss_enabled_mprotection) {
		unsigned int id = get_lowest_page_from_partition_id(1);
		return mprotect(get_page_ptr_from_idx(lid,  id), size, PROT_READ | PROT_WRITE);
	}
	else 
		return untrack_memory((unsigned long) mem_areas[lid], size);
}

int flush(unsigned int lid, unsigned long size) {

	if(!pdes_config.iss_enabled_mprotection) return -NO_PRTCT_ENABLED;
	return flush_local_tlb((unsigned long) mem_areas[lid], size);
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

unsigned int get_lowest_page_from_partition_id(unsigned int page_id){
	do{
		page_id<<=1;
	}while(page_id  < (PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*2);
	return page_id >>=1;
}

void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id){
	assert(id>=PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE);
	assert(id<PER_LP_PREALLOCATED_MEMORY*2/PAGE_SIZE);
	return ((char*)mem_areas[cur_lp]) + (id-PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*PAGE_SIZE; 
}

/** methods to initialize iss support and set tracking_data struct */ 

void set_tracking_data(tracking_data **data, unsigned long start, unsigned long addr, unsigned long end,
	unsigned int segid, unsigned long len) {

	assert((*data)->buff_addresses != NULL);

	(*data)->base_address 					= start;
	(*data)->subsegment_address 			= addr;
	(*data)->end_address 					= end;
	(*data)->segment_id 					= segid;
	(*data)->len_buf 						= len;
	if((*data)->buff_addresses == NULL)	(*data)->buff_addresses = rsalloc(len * sizeof(unsigned long));	
}


void init_tracking_data(tracking_data **data) {

	*data = rsalloc(sizeof(tracking_data));
	(*data)->base_address 			= 0UL;
	(*data)->subsegment_address 	= 0UL;
	(*data)->end_address 			= 0UL;
	(*data)->segment_id 			= 0UL;
	(*data)->len_buf 				= NUM_PAGES_PER_SEGMENT;
	(*data)->buff_addresses 		= rsalloc((*data)->len_buf * sizeof(unsigned long)); 
}



bool is_next_ckpt_incremental(void) {

	if (pdes_config.checkpointing == PERIODIC_STATE_SAVING)  
		return false;

	/// if too many iss do a full ckpt
	if (iss_states[current_lp].iss_counter++ == pdes_config.ckpt_forced_full_period) {
		/// its time to take a full snapshot
		iss_states[current_lp].iss_counter = 0; 
		return false;
	}

	return true;

}

#if BUDDY == 1
void update_tree(unsigned int cur_id, unsigned int partition_id, unsigned int tgt_partition_size) {

    bool was_dirty = 0;
	unsigned int cur_partition_size = 1;
    unsigned short cur_dirty_ts =  iss_states[current_lp].cur_virtual_ts;

	partition_node_tree_t *tree = &iss_states[current_lp].partition_tree[0];

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

}
#endif

/** signal handler -- with mprotect() and with custom syscall and no page-fault hook */


void dirty(void* addr, size_t size){
	//printf("%u: lp %u %p\n", tid, current_lp, addr);
	unsigned int page_id    	= get_page_idx_from_ptr(current_lp, addr);
	unsigned int cur_id 		= page_id;
    iss_states[current_lp].count_tracked++;
	unsigned int tgt_partition_size = 0;
	unsigned int partition_id = page_id;

#if BUDDY == 1
	update_tree(cur_id, partition_id, tgt_partition_size);
#endif

	partition_id = get_lowest_page_from_partition_id(partition_id);

	iss_states[current_lp].current_incremental_log_size += tgt_partition_size*PAGE_SIZE;

	unguard_memory(current_lp, tgt_partition_size*PAGE_SIZE);
}


void sigsev_tracer_for_dirty(int sig, siginfo_t *func, void *arg){
	assert(sig==SIGSEGV);
    assert(__in_log_full == 0);
	(void)arg;
	dirty(func->si_addr, 1);
}


char * get_page_ptr(unsigned long addr) {

	int i;
	unsigned int page_id, cur_id, segid, subsegid, tgt_partition_size, partition_id;

	char *ptr;

	segid = SEGID(addr, (unsigned long) mem_areas[0], NUM_PAGES_PER_SEGMENT);
	subsegid = SEGID(addr, (unsigned long) mem_areas[segid], NUM_PAGES_PER_MMAP);
	page_id = PAGEID((unsigned long) (mem_areas[segid]+subsegid*PAGE_SIZE), (unsigned long) mem_areas[segid]);
	cur_id = page_id;
    iss_states[current_lp].count_tracked++;
	tgt_partition_size = 0;
	partition_id = page_id;


#if BUDDY == 1
    update_tree(cur_id, partition_id, tgt_partition_size); //TODO: check this
	iss_states[current_lp].current_incremental_log_size += tgt_partition_size*PAGE_SIZE;
#else
	iss_states[current_lp].current_incremental_log_size += PAGE_SIZE;
#endif
	
	ptr = PAGEPTR(mem_areas[current_lp], page_id);
	//printf("[lp %u] [get_page_ptr] buff[i] %lu segid %lu subsegid %lu \t page-id %u \t ptr %p\n",current_lp, addr, segid, subsegid, page_id, ptr);


	return ptr;

}



/** methods to use klm through ioctl */

void init_segment_monitor_support(tracking_data *data) {

	assert(data != NULL);
	assert(device_fd != -1);

  #if VERBOSE == 1
	printf(" [LP: %u] [init_segment_monitor_support] base_addr %lu subsegment_address %lu segid %ld\n", 
		current_lp, data->base_address, data->subsegment_address, data->segment_id);
  #endif


	ioctl(device_fd, TRACKER_INIT, data);
}

void open_tracker_device(const char *path, unsigned long mode) {

	device_fd = open(path, mode);
    if (device_fd == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        abort();
    }
    ioctl(device_fd, TRACKER_SET_SEGSIZE, PER_LP_PREALLOCATED_MEMORY);

}


void close_tracker_device(void) {

	if (close(device_fd) == -1) {
		fprintf(stderr, "%s\n", strerror(errno));
        abort();
	}
  
}

tracking_data *get_fault_info(unsigned int lid) {

	tracking_data *local_data = t_data[lid];
	unsigned long len;
	unsigned long *buff;
	unsigned long segid;
	local_data->base_address = (unsigned long) mem_areas[0];
	local_data->subsegment_address = (unsigned long) mem_areas[lid];
	local_data->end_address = (unsigned long) (mem_areas[lid]+MAX_MMAP*NUM_MMAP);
	local_data->len_buf = (unsigned long) NUM_PAGES_PER_MMAP;
	if(local_data->buff_addresses == NULL) local_data->buff_addresses = rsalloc(local_data->len_buf * sizeof(unsigned long));
	segid = SEGID(mem_areas[lid], mem_areas[0], NUM_PAGES_PER_SEGMENT);
	

	ioctl(device_fd, TRACKER_GET, local_data);

	if (local_data != NULL) 
		return local_data;
}


/** incremental state saving facilities */

partition_log *create_log(simtime_t ts, partition_log *prev_log, unsigned long address) {

	partition_log * cur_log = (partition_log*) rsalloc(sizeof(partition_log));
	cur_log->size = PAGE_SIZE;
	cur_log->next = prev_log;
	cur_log->ts = ts;
	cur_log->addr = get_page_ptr(address);

	return cur_log;
}


partition_log *log_incremental(unsigned int cur_lp, simtime_t ts) {

#if BUDDY == 1
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int cur_partition_size, tgt_partition_size, cur_id, tgt_id, first_dirty_size = 0, first_dirty = 0;
    unsigned int cur_start;
    unsigned int cur_end = end;
	partition_log *cur_log = NULL, *prev_log = NULL;
    unsigned int or_size = iss_states[cur_lp].current_incremental_log_size;
    int prev = 0;
    unsigned int last_restart = 0;
    unsigned short cur_dirty_ts = iss_states[cur_lp].cur_virtual_ts;
	   
    unsigned int tmp = 0, last_computed = 0;
    unsigned int prev_end;
    unsigned int prev_first_dirty_end = end;

    //TODO: add buddy scheme WITH protection → for each address in buffer do tree scan


	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 

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
            
            //TODO: call get_fault_info and get_page_ptr for ckpt IF SIGNAL==0
			tgt_id = get_lowest_page_from_partition_id(tgt_id);
			cur_log = (partition_log*) rsalloc(sizeof(partition_log));
			cur_log->size = tgt_partition_size*PAGE_SIZE;
			cur_log->next = prev_log;
			cur_log->ts = ts;
			cur_log->addr = get_page_ptr_from_idx(cur_lp, tgt_id);
			cur_log->log = rsalloc(cur_log->size);
			prev_log = cur_log; 

			//printf("[log_incremental] CKPT tgt_id %u \t addr %p \t cur_log %p \t log %p \t size\n", 
			//	tgt_id, cur_log->addr, cur_log, cur_log->log, iss_states[cur_lp].current_incremental_log_size);

			iss_states[cur_lp].current_incremental_log_size -= cur_log->size;
			memcpy(cur_log->log, cur_log->addr, cur_log->size);
//			assert(start == tgt_id);

		} else {

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
	//assert(iss_states[cur_lp].current_incremental_log_size == 0);

#else

	partition_log *cur_log = NULL, *prev_log = NULL;
	tracking_data *data = get_fault_info(cur_lp);
	unsigned long len, segid;
	unsigned long *buff;
	int i;
	if (data != NULL) {
		len = data->len_buf;
		buff = rsalloc(sizeof(unsigned long) * len);
		//iss_states[cur_lp].current_incremental_log_size += len*PAGE_SIZE;
		if (buff != NULL) buff = data->buff_addresses;
		for (i = 0; i < len; i++) {

			cur_log = create_log(ts, prev_log, buff[i]);
			cur_log->log = rsalloc(cur_log->size);
			prev_log = cur_log;	
			
			//printf("[log_incremental] CKPT \t addr %p \t long addr %lu \t cur_log %p \t log %p \t size %lu\n", 
			//	cur_log->addr, (unsigned long)prev_log->addr , prev_log, prev_log->log, iss_states[cur_lp].current_incremental_log_size);

			iss_states[cur_lp].current_incremental_log_size -= cur_log->size;
			memcpy(cur_log->log, cur_log->addr, cur_log->size);
		}


	} else { printf("[log_incremental] no data\n");}



#endif

	
	return prev_log;

}

/**
* This function restores the incremental ckpts
*
*
* @param cur Ptr to a log
*/
void log_incremental_restore(partition_log *cur) {

	while(cur){
		memcpy(cur->addr, cur->log, cur->size);
		cur = cur->next;
	}

}


/**
* This function destroys all the incremental ckpts
*
*
* @param cur Ptr to a log
*/
void log_incremental_destroy_chain(partition_log *cur){
	partition_log *next = NULL;
	while(cur){
		next = cur->next;
		rsfree(cur->log);
		rsfree(cur);
		cur = next;
	}	
}



/** init incremental state saving support */

/**
* This function initializes the incremental ckpt support globally
*
*
* @param lps The number of LPs
*/
void init_incremental_checkpointing_support(unsigned int lps) {

	uint i;

  #if VERBOSE == 1
	printf("[init_incremental_checkpointing_support] %u \n", lps);
  #endif

	t_data = malloc(sizeof(tracking_data) * lps);
	if (t_data != NULL) {
	for (i = 0; i < lps; i++)
		init_tracking_data(&t_data[i]);
	}

	/// init model PER-LP (iss_metadata and model)
  #if BUDDY == 1
	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*lps + (2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*sizeof(partition_node_tree_t)*lps);
  #else
	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*lps);
  #endif
	iss_costs_model.mprotect_cost_per_page = 1;
	iss_costs_model.log_cost_per_page = 100; 

	/// if signaling mechanism is enabled
	if (pdes_config.iss_signal_mprotect) {
		struct sigaction action;
		action.sa_sigaction = sigsev_tracer_for_dirty;
		action.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &action, NULL);
	}



}

/**
* This function initializes the incremental ckpt support for each lp 
*
*
* @param lp The logical process' identifier
*/
void init_incremental_checkpoint_support_per_lp(unsigned int lp){

  #if BUDDY == 1
	bzero(iss_states+lp, sizeof(lp_iss_metadata) + (2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*sizeof(partition_node_tree_t));
  #else 
	bzero(iss_states+lp, sizeof(lp_iss_metadata));
  #endif

	if (pdes_config.iss_enabled_mprotection) {
		/// fill tracking_data struct
		unsigned int segid = SEGID(mem_areas[lp], mem_areas[0], NUM_PAGES_PER_SEGMENT);
		set_tracking_data(&t_data[lp], (unsigned long) mem_areas[0], (unsigned long) mem_areas[lp],
			(unsigned long) mem_areas[lp] + MAX_MMAP*NUM_MMAP, segid, NUM_PAGES_PER_SEGMENT);
	  //#if VERBOSE == 1
		printf("[LP: %u] base_addr %lu subsegment_address %lu segid %lu\n", lp, t_data[lp]->base_address, t_data[lp]->subsegment_address, t_data[lp]->segment_id);
	  //#endif


	/// register segment into the hashtable
	init_segment_monitor_support(t_data[lp]);
	}


	iss_first_run_model(current_lp); 


}
