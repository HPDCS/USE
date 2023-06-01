#ifndef __ISS_H_
#define __ISS_H_

#include <mm.h>

/// This struct keeps metadata of a partition log
typedef struct __partition_log{
	size_t size;
	struct __partition_log *next;
	simtime_t ts;
	char *addr;
	char *log;
}partition_log;

/// This struct keeps parameters for the iss cost model
typedef struct __model{
	double mprotect_cost_per_page;
	double log_cost_per_page;
}model_t;


/// This struct keeps runtime info for an admittable partition of the state segment
typedef struct __partition_tree_node{
	double cost;
	unsigned long long access_count;
	char valid;
	char dirty;
}partition_node_tree_t;


/// This struct keeps all metadata for incremental state saving of a model state
typedef struct __per_lp_iss_metadata{
	partition_node_tree_t partition_tree[2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE];
	size_t current_incremental_log_size;
	unsigned int iss_counter;
	unsigned int total_access_count;
}lp_iss_metadata;

bool is_next_ckpt_incremental();

void init_incremental_checkpoint_support(unsigned int num_lps);
void init_incremental_checkpoint_support_per_lp(unsigned int lp);

void iss_first_run_model(unsigned int cur_lp);
void iss_update_model(unsigned int cur_lp);
void iss_protect_all_memory(unsigned int cur_lp);
void iss_unprotect_all_memory(unsigned int cur_lp);

void log_incremental_destroy_chain(partition_log *cur);

partition_log* log_incremental(unsigned int, simtime_t ts);
void log_incremental_restore(partition_log *cur);


#endif