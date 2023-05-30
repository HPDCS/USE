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
}lp_iss_metadata;

bool is_next_ckpt_incremental();

void run_model(unsigned int cur_lp);

partition_log* log_incremental(unsigned int cur_lp, simtime_t ts);
void log_incremental_restore(unsigned int cur_lp, partition_log *cur);

#endif