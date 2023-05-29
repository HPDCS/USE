#ifndef __ISS_H_
#define __ISS_H_

#include <mm.h>

typedef struct __partition_tree_node{
	double cost;
	unsigned long long access_count;
	char valid;
	char dirty;
}partition_node_tree_t;

typedef struct __per_lp_iss_metadata{
	partition_node_tree_t partition_tree[2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE];
	unsigned int iss_counter;
}lp_iss_metadata;

lp_iss_metadata *iss_states;


bool is_next_ckpt_incremental();

#endif