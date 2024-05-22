#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

#include <ROOT-Sim.h>

#include <mm.h>
#include <dymelor.h>
#include <segment.h>
#include <incremental_state_saving.h>
#include <signal.h>


tracking_data **t_data;
int device_fd;

extern void **mem_areas; /// pointers to the lp segment

iss_func iss_log;

bitmap **dirty_pages;

lp_iss_metadata *iss_states; /// runtime iss metadata for each lp

__thread int __in_log_full = 0;

unsigned long get_iss_size(unsigned int lp) { return iss_states[lp].current_incremental_log_size; }
void set_iss_size(unsigned int lp, unsigned long size) { iss_states[lp].current_incremental_log_size = size; }

void sigsev_tracer_for_dirty(int sig, siginfo_t *func, void *arg){
	assert(sig==SIGSEGV);
    assert(__in_log_full == 0);
	(void)arg;
	dirty(func->si_addr, PAGE_SIZE, 0);
}

/** syscalls */
int track_memory(unsigned long address, size_t size){
	return syscall(PROTECT_MEM,address, size);
}

int untrack_memory(unsigned long address, size_t size){
	return syscall(UNPROTECT_MEM,address, size);
}

int flush_local_tlb(unsigned int lid, size_t size){
	return syscall(FLUSH_LOCAL_TLB, (unsigned long) mem_areas[lid], size);
}

/** syscalls' wrappers */


int unguard_memory(unsigned int lid, unsigned long size, unsigned int page_id) {
	
	assert(size <= PER_LP_PREALLOCATED_MEMORY);

	if(!pdes_config.iss_enabled_mprotection) {
		//printf("[unguard_memory lp %u] -- ptr %p -- size %lu\n", lid, get_page_ptr_from_idx(lid, id), size);
		return mprotect(get_page_ptr_from_idx(lid, page_id), size, PROT_READ | PROT_WRITE);
	}
	else {
		/// CASE WITH CUSTOM SYSCALL KLM AND NO PAGE FAULT HOOK
		return untrack_memory((unsigned long) (mem_areas[lid] + page_id*PAGE_SIZE), size);
	}
}



int guard_all_memory(unsigned int lid) {

	if(!pdes_config.iss_enabled_mprotection) {
		return mprotect((void *)mem_areas[lid], PER_LP_PREALLOCATED_MEMORY, PROT_READ);
	}
	else {
		return track_memory((unsigned long) mem_areas[lid], PER_LP_PREALLOCATED_MEMORY);
	}
}

int unguard_all_memory(unsigned int lid) {
	
	if(!pdes_config.iss_enabled_mprotection) {
		return mprotect((void *)mem_areas[lid], PER_LP_PREALLOCATED_MEMORY, PROT_READ | PROT_WRITE);
	}
	else {
		return untrack_memory((unsigned long) mem_areas[lid], PER_LP_PREALLOCATED_MEMORY);
	}
}


int flush(unsigned int lid, unsigned long size) {

	if(!pdes_config.iss_enabled_mprotection) return -NO_PRTCT_ENABLED;
	return flush_local_tlb((unsigned long) mem_areas[lid], size);
}



int get_page_idx_from_ptr(unsigned int cur_lp, void *addr){
	unsigned long long base_addr = (unsigned long long)(mem_areas[cur_lp]);
	unsigned long long pg_addr = ((unsigned long long)addr) & (~ (PAGE_SIZE-1));
	//printf("[lp %u] pg_addr % NUM_PAGES_PER_SEGMENT %lu\n", cur_lp, pg_addr % PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE);
	assert(pg_addr >= base_addr);
	assert(pg_addr <  (base_addr+PER_LP_PREALLOCATED_MEMORY));
	unsigned long long offset = pg_addr - base_addr;
	assert(offset < PER_LP_PREALLOCATED_MEMORY);
	//printf("[lp %u] [get_page_idx_from_ptr] address %lu - %p \t offset %lu -- %u -- %u -- bitmap length %lu\n", 
	//	cur_lp, (unsigned long long)addr, addr, offset, offset/PAGE_SIZE, offset/PAGE_SIZE + PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE, dirty_pages[cur_lp]->actual_len);
	return (unsigned int) offset/PAGE_SIZE;
}

unsigned int get_lowest_page_from_partition_id(unsigned int page_id){
	do{
		page_id<<=1;
	}while(page_id  < (PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*2);
	return page_id >>=1;
}

void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id){
	assert(id>=0);
	assert(id<PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE);
	return ((char*)mem_areas[cur_lp] + id*PAGE_SIZE); 
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

void reset_tracking_data(tracking_data **data) {

	unsigned long len;
	uint i;

	len = (*data)->len_buf;

    for (i = 0; i < len; i++) {
    	if ((*data)->buff_addresses != NULL) (*data)->buff_addresses[i] = 0UL;
    }

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



void dirty(void* addr, size_t size, unsigned int cur_lp){
	//fprintf(stderr, "[dirty] %u: lp %u %p\n", tid, cur_lp, addr);
	unsigned int page_id;

	if (pdes_config.iss_signal_mprotect) 
		cur_lp = current_lp;

	page_id    	= get_page_idx_from_ptr(cur_lp, addr);
    iss_states[cur_lp].count_tracked++;

	if (!get_bit(dirty_pages[cur_lp], page_id)) {
		iss_states[cur_lp].current_incremental_log_size += size;
		//printf("[lp %u] BUFFER ADDRESS i %u \t address %llu - %p\n", cur_lp, page_id, (unsigned long long) addr, (void *) addr);
		set_bit(dirty_pages[cur_lp], page_id);
	}

	if (pdes_config.iss_signal_mprotect)
		unguard_memory(cur_lp, size, page_id);
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
	segid = lid;
	

	ioctl(device_fd, TRACKER_GET, local_data);

	return local_data;
}


/** incremental state saving facilities */


void mark_dirty_pages(unsigned int cur_lp) {

	tracking_data *data = get_fault_info(cur_lp);
	unsigned long len;
	unsigned long *buff;
	int j;
	if (data != NULL) {

		len = data->len_buf;
		//buff = rsalloc(sizeof(unsigned long) * len);
		if (data->buff_addresses != NULL && buff != NULL) buff = data->buff_addresses;

		for (j = 0; j < len; j++) {
			dirty((void *) data->buff_addresses[j], PAGE_SIZE, cur_lp);
		} ///end for
		
	} ///end if data != NULL

} 


partition_log * log_incremental_no_tree(unsigned int cur_lp, simtime_t ts) {

	partition_log *cur_log = NULL, *prev_log = NULL;
	uint i;
	

	for (i = 0; i <= dirty_pages[cur_lp]->max_idx; i++) {

		if (get_bit(dirty_pages[cur_lp], i)) {

			cur_log = (partition_log*) rsalloc(sizeof(partition_log));
			cur_log->size = PAGE_SIZE;
			cur_log->next = prev_log;
			cur_log->ts = ts;
			cur_log->addr = ((char*)mem_areas[cur_lp] + i*PAGE_SIZE);
			cur_log->log = rsalloc(cur_log->size);
			prev_log = cur_log; 


  #if VERBOSE == 1
			printf("[lp %u] BITMAP ADDRESS i %d \t address %lu - %p is address in page %lu \n", cur_lp, i , 
				(unsigned long )cur_log->addr, (void *) cur_log->addr,
				((unsigned long )cur_log->addr >= (unsigned long) mem_areas[cur_lp] + i*PAGE_SIZE && 
					(unsigned long )cur_log->addr <= (unsigned long) mem_areas[cur_lp] + i*PAGE_SIZE + PAGE_SIZE));
  #endif
			

  #if VERBOSE == 1
			printf("[lp %u] [log_incremental] CKPT tgt_id %u \t addr %p \t cur_log %p \t log %p \t ts %f \t size %lu\n", 
				cur_lp, i, cur_log->addr, cur_log, cur_log->log, cur_log->ts, iss_states[cur_lp].current_incremental_log_size);
  #endif


			iss_states[cur_lp].current_incremental_log_size -= cur_log->size;
			memcpy(cur_log->log, cur_log->addr, cur_log->size);
			reset_bit(dirty_pages[cur_lp], i);
		}

	}


	//if (prev_log != NULL) printf("[lp %u] [log_incremental_no_tree] log done %x\n", cur_lp, prev_log->log);
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
	 #if VERBOSE == 1	
		printf("lp %u : [log_incremental_restore] cur %x -- log %x \t ts %f \n", current_lp, cur, cur->log, cur->ts);
	 #endif	
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
		//printf("lp %u : [log_incremental_destroy_chain] cur %x -- log %x \t ts %f \n", current_lp, cur, cur->log, cur->ts);
		next = cur->next;
		rsfree(cur->log);
		rsfree(cur);
		cur = next;
	}	
}


void iss_log_incremental_reset(unsigned int lp){


    iss_states[lp].current_incremental_log_size = 0;
    iss_states[lp].count_tracked = 0;
    
    
    if(iss_states[lp].cur_virtual_ts == 65000){
        iss_states[lp].cur_virtual_ts = 0;
    }

    if (!iss_states[lp].first_log && pdes_config.iss_enabled_mprotection) {
		
		if (t_data[lp] != NULL) reset_tracking_data(&t_data[lp]);

		if (iss_states[lp].empty_klm_buffer) iss_states[lp].empty_klm_buffer = 0;

	}
    
    iss_states[lp].cur_virtual_ts += 1;

	if (iss_states[lp].first_log) iss_states[lp].first_log = 0;

    
}


/** init incremental state saving support */

/**
* This function initializes the incremental ckpt support globally
*
*
* @param lps The number of LPs
*/
void init_incremental_checkpointing_support(unsigned int lps) {


  #if VERBOSE == 1
	printf("[init_incremental_checkpointing_support] %u \n", lps);
  #endif

	/// init model PER-LP (iss_metadata and model)
	iss_states = (lp_iss_metadata*)rsalloc(sizeof(lp_iss_metadata)*lps);

	uint i;

	/// init tracking dirty memory mechanism
	dirty_pages = rsalloc(lps * sizeof(bitmap));

	printf("PER_LP_PREALLOCATED_MEMORY %lu \t NUM_PAGES_PER_SEGMENT %lu \t NUM_PAGES_PER_MMAP %lu \t MAX_MMAP*NUM_MMAP %lu\n",
	 PER_LP_PREALLOCATED_MEMORY, NUM_PAGES_PER_SEGMENT, NUM_PAGES_PER_MMAP, MAX_MMAP*NUM_MMAP);

	/// install log incremental handler
	iss_log.iss_log_inc = log_incremental_no_tree;

	// if klm is enabled alloc tracking_data struct
	if (pdes_config.iss_enabled_mprotection) {

		t_data = rsalloc(sizeof(tracking_data) * lps);
		if (t_data != NULL) {
		for (i = 0; i < lps; i++)
			init_tracking_data(&t_data[i]);
		}
	}


	/// if signaling mechanism is enabled install signal handler
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

	bzero(iss_states+lp, sizeof(lp_iss_metadata));

	iss_states[lp].cur_virtual_ts = 1;

	/// if klm is enabled setup tracking_data struct entries 
	if (pdes_config.iss_enabled_mprotection) {
		/// fill tracking_data struct
		set_tracking_data(&t_data[lp], (unsigned long) mem_areas[0], (unsigned long) mem_areas[lp],
			(unsigned long) mem_areas[lp] + PER_LP_PREALLOCATED_MEMORY - 1, lp, PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE);
	  #if VERBOSE == 1
		printf("[LP: %u] base_addr %lu subsegment_address %lu segid %lu\n", lp, t_data[lp]->base_address, t_data[lp]->subsegment_address, t_data[lp]->segment_id);
	  #endif


		/// register segment into the hashtable
		init_segment_monitor_support(t_data[lp]);

	}


}
