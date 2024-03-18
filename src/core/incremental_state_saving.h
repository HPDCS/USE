#ifndef __ISS_H_
#define __ISS_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <pthread.h>
#include <assert.h>

#include <segment.h>


#define PROTECT_MEM 134 //this depends on what the kernel tells you when mounting the vtpmo module
#define UNPROTECT_MEM 156 //this depends on what the kernel tells you when mounting the vtpmo module
#define FLUSH_LOCAL_TLB 174 //this depends on what the kernel tells you when mounting the vtpmo module

#define NO_PRTCT_ENABLED 1

#define BE_BUFF_SIZE NUM_MMAP


#define TRACKER_INIT			(1U << 2) ///ioctl cmd for initialization of the support
#define TRACKER_GET				(1U << 3) ///ioctl cmd for the retrieval request
#define TRACKER_DUMP			(1U << 4) ///ioctl cmd for debugging purposes (serial only)
#define TRACKER_SET_SEGSIZE		(1U << 5) ///ioctl cmd for setting segment size

#define SEGID(addr, base, size) ({unsigned int id = ((addr-base)/PAGE_SIZE + size)/size - 1; id;})
#define PAGEID(addr, base) ({unsigned int id = (unsigned int) ((addr- base)/PAGE_SIZE + PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE); id;})
#define PAGEPTR(addr, pageid) ({char *ptr = (char*)addr + (pageid-PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE)*PAGE_SIZE; ptr;})

/* user data struct to pass data back and forth user/kernel space */
typedef struct _tracking_data {
	unsigned long base_address; 		/// base address of segment
	unsigned long subsegment_address; 	/// base address of subsegment
	unsigned long end_address;			/// end address of segment
	unsigned long segment_id;			/// segment id
	unsigned long *buff_addresses;		/// buffer of addresses
	unsigned long len_buf;				/// requested buffer's lenght
} tracking_data;	

/** This struct keeps metadata of a partition log */
typedef struct __partition_log{
	size_t size;
	struct __partition_log *next;
	simtime_t ts;
	char *addr;
	char *log;
}partition_log;


/// This struct keeps parameters for the iss cost model
typedef struct __model{
	float mprotect_cost_per_page;
	float log_cost_per_page;
}model_t;

/// This struct keeps runtime info for an admittable partition of the state segment
typedef struct __partition_tree_node{
	float cost;
	int access_count;
	char valid[2];
	unsigned short dirty;
}partition_node_tree_t;


/// This struct keeps all metadata for incremental state saving of a model state
typedef struct __per_lp_iss_metadata{
  #if BUDDY == 1
	//partition_node_tree_t partition_tree[2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE];
  #endif
	ssize_t current_incremental_log_size;
	int iss_counter;
    int iss_model_round;
    int count_tracked;
    int disabled;
    unsigned short cur_virtual_ts;
    char current_model;
  #if BUDDY == 1
	partition_node_tree_t partition_tree[]; //when alloc per_lp_iss_metadata add 2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE
  #endif
}lp_iss_metadata;

extern tracking_data **t_data;

/* file descriptor of the device file */
extern int device_fd;

extern lp_iss_metadata *iss_states; /// runtime iss metadata for each lp
extern model_t iss_costs_model;	 /// runtime tuning of the cost model 

/** methods for incremental state saving support */
void init_incremental_checkpointing_support(unsigned int threads, unsigned int lps);
void init_tracking_data(tracking_data **);
void set_tracking_data(tracking_data **data, unsigned long start, unsigned long addr, unsigned long end,
										unsigned int segid, unsigned long len);
void init_incremental_checkpoint_support_per_lp(unsigned int lp);

/** methods for incremental state saving */
bool is_next_ckpt_incremental();
partition_log *log_incremental(unsigned int lid, simtime_t ts);
void log_incremental_restore(partition_log *cur);
void log_incremental_destroy_chain(partition_log *cur);

tracking_data *get_fault_info(unsigned int lid);
partition_log* mark_dirty_pages(unsigned long buff, unsigned long size, partition_log *prev_log);

void init_segment_monitor_support(tracking_data *data);


void sigsev_tracer_for_dirty(int sig, siginfo_t *func, void *arg);
void dirty(void *, size_t);

/** methods for model management */
void iss_first_run_model(unsigned int current_lp); 
void iss_log_incremental_reset(unsigned int lp);
void iss_update_model(unsigned int cur_lp);
float estimate_cost(size_t size, float probability);


/** syscalls wrapper */
int guard_memory(unsigned int lid, unsigned long size);
int unguard_memory(unsigned int lid, unsigned long size);
int flush(unsigned int lid, unsigned long size);

/** syscalls */
int track_memory(unsigned long address, size_t size);
int untrack_memory(unsigned long address, size_t size);
int flush_local_tlb(unsigned long address, size_t size);


#endif