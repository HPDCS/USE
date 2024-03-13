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

//lp_iss_metadata *iss_states; /// runtime iss metadata for each lp
//model_t iss_costs_model;	 /// runtime tuning of the cost model 

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

	if(!pdes_config.iss_enabled_mprotection) return -NO_PRTCT_ENABLED;
	assert(size <= PER_LP_PREALLOCATED_MEMORY);
	return track_memory((unsigned long) mem_areas[lid], size);
}

int unguard_memory(unsigned int lid, unsigned long size) {

	if(!pdes_config.iss_enabled_mprotection) return -NO_PRTCT_ENABLED;
	assert(size <= PER_LP_PREALLOCATED_MEMORY);
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

partition_log *log_incremental(unsigned int lid, simtime_t ts) {

	partition_log *ckpt;

	return ckpt;
}


void log_incremental_restore(partition_log *cur) {


}


void mark_dirty_pages(unsigned long *buff, unsigned long size) {

	int i;
	unsigned int page_id, cur_id, tgt_partition_size, cur_partition_size, partition_id;
	bool was_dirty;
	unsigned short cur_dirty_ts;
	unsigned long long pg_addr;
	
	for (i=0; i < size; i++) {
		//todo: do some things to mark dirty pages
		page_id = PAGEID(buff[i], (unsigned long) mem_areas[current_lp]);
		printf("[mark_dirty_pages] page-id %u\n", page_id);
		cur_id = page_id;
		partition_node_tree_t *tree = &iss_states[current_lp].partition_tree[0];
	    iss_states[current_lp].count_tracked++;
		tgt_partition_size = 0;
		cur_partition_size = 1;
		partition_id = page_id;
	    was_dirty = 0;
	    cur_dirty_ts =  iss_states[current_lp].cur_virtual_ts;

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
	}

}


/** methods to use klm through ioctl */

void init_segment_monitor_support(tracking_data *data) {

	assert(data != NULL);
	assert(device_fd != -1);

	printf("[init_segment_monitor_support] base_addr %lu subsegment_address %lu segid %ld\n", 
		data->base_address, data->subsegment_address, data->segment_id);

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


tracking_data *get_fault_info(unsigned int lid) {

	tracking_data *local_data = t_data[lid];
	unsigned long len;
	unsigned long *buff;
	local_data->base_address = (unsigned long) mem_areas[0];
	local_data->subsegment_address = (unsigned long) mem_areas[lid];
	local_data->end_address = (unsigned long) (mem_areas[lid]+MAX_MMAP*NUM_MMAP);
	local_data->len_buf = (unsigned long) NUM_PAGES_PER_MMAP;
	if(local_data->buff_addresses == NULL) local_data->buff_addresses = malloc(local_data->len_buf * sizeof(unsigned long));
	local_data->segment_id = SEGID(mem_areas[lid], mem_areas[0], NUM_PAGES_PER_SEGMENT);

	ioctl(device_fd, TRACKER_GET, &local_data[lid]);

	if (t_data != NULL) {
		len = local_data->len_buf;
		buff = rsalloc(sizeof(unsigned long) * len);
		mark_dirty_pages(buff, len);
		return local_data;
	}

	return local_data;
}

/**  only when protect() is used -- maybe use compilation flags */


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
	untrack_memory((unsigned long) get_page_ptr_from_idx(current_lp, partition_id), tgt_partition_size*PAGE_SIZE);
}


void sigsev_tracer_for_dirty(int sig, siginfo_t *func, void *arg){
	assert(sig==SIGSEGV);
    assert(__in_log_full == 0);
	(void)arg;
	dirty(func->si_addr, 1);
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

void init_incremental_checkpointing_support(unsigned int threads, unsigned int lps) {

	uint i;
	printf("[init_incremental_checkpointing_support] %u \n", lps);
	t_data = malloc(sizeof(tracking_data) * lps);
	if (t_data != NULL) {
	for (i = 0; i < lps; i++)
		init_tracking_data(&t_data[i]);
	}

	/// init model PER-LP (iss_metadata and model)
	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*lps);
	iss_costs_model.mprotect_cost_per_page = 1; //TODO: costo per protect ?
	iss_costs_model.log_cost_per_page = 100; 

	if (pdes_config.iss_signal_mprotect) {
		struct sigaction action;
		action.sa_sigaction = sigsev_tracer_for_dirty;
		action.sa_flags = SA_SIGINFO;
		sigaction(SIGSEGV, &action, NULL);
	}



}

void init_incremental_checkpoint_support_per_lp(unsigned int lp){

	bzero(iss_states+lp, sizeof(lp_iss_metadata));
	
	//INCR: fill tracking_data struct with addresses and then call ioctl(fd, TRACKER_INIT, t_data)
	unsigned int segid = SEGID(mem_areas[lp], mem_areas[0], NUM_PAGES_PER_SEGMENT);
	set_tracking_data(&t_data[lp], (unsigned long) mem_areas[0], (unsigned long) mem_areas[lp],
		(unsigned long) mem_areas[lp] + MAX_MMAP*NUM_MMAP, segid, NUM_PAGES_PER_SEGMENT);

	printf("base_addr %lu subsegment_address %lu segid %lu\n", t_data[lp]->base_address, t_data[lp]->subsegment_address, t_data[lp]->segment_id);
	
	//INCR: ioctl(fd, TRACKER_INIT, t_data)
	init_segment_monitor_support(t_data[lp]);

	iss_first_run_model(current_lp); 


}