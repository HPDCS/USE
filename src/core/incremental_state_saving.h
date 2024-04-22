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
#include <bitmap.h>


#define PROTECT_MEM 134 //this depends on what the kernel tells you when mounting the vtpmo module
#define UNPROTECT_MEM 156 //this depends on what the kernel tells you when mounting the vtpmo module
#define FLUSH_LOCAL_TLB 174 //this depends on what the kernel tells you when mounting the vtpmo module

#define NO_PRTCT_ENABLED 1

#define BE_BUFF_SIZE 256


#define TRACKER_INIT			(1U << 2) ///ioctl cmd for initialization of the support
#define TRACKER_GET				(1U << 3) ///ioctl cmd for the retrieval request
#define TRACKER_DUMP			(1U << 4) ///ioctl cmd for debugging purposes (serial only)
#define TRACKER_SET_SEGSIZE		(1U << 5) ///ioctl cmd for setting segment size

#define SEGID(addr, base, size) ({unsigned int id = (abs(addr-base)/PAGE_SIZE + size)/size - 1; id;})
#define PAGEID(addr, base) ({unsigned int id = (unsigned int) (abs(addr- base)/PAGE_SIZE + PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE); id;})
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
	ssize_t current_incremental_log_size;
	int iss_counter;
    int iss_model_round;
    int count_tracked;
    int disabled;
    unsigned short cur_virtual_ts;
    char current_model;
}lp_iss_metadata;


typedef struct _iss_func {
	partition_log *(*iss_log_inc)(unsigned int cur_lp, simtime_t ts);
} iss_func;

extern iss_func iss_log;

extern bitmap **dirty_pages;

extern tracking_data **t_data;

/* file descriptor of the device file */
extern int device_fd;

extern lp_iss_metadata *iss_states; /// runtime iss metadata for each lp
extern model_t iss_costs_model;	 /// runtime tuning of the cost model 

/** methods for incremental state saving support */
void init_incremental_checkpointing_support(unsigned int lps);
void init_tracking_data(tracking_data **);
void set_tracking_data(tracking_data **data, unsigned long start, unsigned long addr, unsigned long end,
										unsigned int segid, unsigned long len);
void init_incremental_checkpoint_support_per_lp(unsigned int lp);

/** methods for incremental state saving */
bool is_next_ckpt_incremental();

partition_log *log_incremental_no_tree(unsigned int cur_lp, simtime_t ts);

void log_incremental_restore(partition_log *cur);
void log_incremental_destroy_chain(partition_log *cur);

tracking_data *get_fault_info(unsigned int lid);
char* get_page_ptr(unsigned long addr);

void init_segment_monitor_support(tracking_data *data);


extern void dirty(void*, size_t, unsigned int);

#if BUDDY == 1
/** methods for model management */
void iss_first_run_model(unsigned int current_lp); 
void iss_update_model(unsigned int cur_lp);
float estimate_cost(size_t size, float probability);
#endif
void iss_log_incremental_reset(unsigned int lp);

extern unsigned long get_iss_size(unsigned int lp);
extern void set_iss_size(unsigned int lp, unsigned long size);

int get_page_idx_from_ptr(unsigned int cur_lp, void *addr);
unsigned int get_lowest_page_from_partition_id(unsigned int page_id);
void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id);


/** syscalls wrapper */
int guard_memory(unsigned int lid, unsigned long size);
int guard_all_memory(unsigned int lid);
int unguard_memory(unsigned int lid, unsigned long size, unsigned int pageid);
int unguard_all_memory(unsigned int lid);
int flush(unsigned int lid, unsigned long size);

/** syscalls */
int track_memory(unsigned long address, size_t size);
int untrack_memory(unsigned long address, size_t size);
int flush_local_tlb(unsigned int lid, size_t size);


#endif