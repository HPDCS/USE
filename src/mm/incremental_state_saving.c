#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>

#include <ROOT-Sim.h>

#include <incremental_state_saving.h>

extern void **mem_areas;


typedef struct __partition_log{
	size_t size;
	struct __partition_log *next;
	simtime_t ts;
	char *addr;
	char *log;
}partition_log;


typedef struct __model{
	double mprotect_cost_per_page;
	double log_cost_per_page;
}model_t;


/*typedef struct __partition_tree_node{
	double cost;
	unsigned long long access_count;
	char valid;
	char dirty;
}partition_node_tree_t;

typedef struct __per_lp_iss_metadata{
	partition_node_tree_t partition_tree[2*PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE];
	unsigned int iss_counter;
}lp_iss_metadata;

lp_iss_metadata *iss_states;*/
model_t iss_costs_model;

void init_incremental_checkpoint_support(unsigned int num_lps){
	iss_states = (lp_iss_metadata*)malloc(sizeof(lp_iss_metadata)*num_lps);
	iss_costs_model.mprotect_cost_per_page = 1;
	iss_costs_model.log_cost_per_page = 100;
}

void init_incremental_checkpoint_support_per_thread(unsigned int lp){
	bzero(iss_states+lp, sizeof(lp_iss_metadata));
}

unsigned int get_page_id_from_address(unsigned int cur_lp, void *addr){
	unsigned long long base_addr = (unsigned long long)(mem_areas[cur_lp]);
	unsigned long long pg_addr = ((unsigned long long)addr) & (~ (PAGE_SIZE-1));
	unsigned long long offset = pg_addr - base_addr;
	assert(offset < PER_LP_PREALLOCATED_MEMORY);
	return (unsigned int) offset/PAGE_SIZE + PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
}

void* get_page_ptr_from_id(unsigned int cur_lp, unsigned int id){
	return ((char*)mem_areas[cur_lp]) + id*PAGE_SIZE; 
}

unsigned int get_lowest_page_from_id(unsigned int page_id){
	while((page_id << 1) > (PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE))
		page_id<<=1;
	return page_id >>=1;
}

double estimate_cost(size_t size, double probability){
	return size*iss_costs_model.mprotect_cost_per_page + probability*iss_costs_model.log_cost_per_page;
}



/* Marks the page as dirty */
void dirty(void* addr, size_t size){
	unsigned int page_id      = get_page_id_from_address(current_lp, addr);
	unsigned int cur_id = page_id;
	partition_node_tree_t *tree = &iss_states[current_lp].partition_tree[0];

	unsigned int tgt_partition_size = 0;
	unsigned int partition_id = page_id;
	unsigned int cur_partition_size = 1;

	/// increase access counters for cost model and dirty tracking
	while(cur_id > 0){
		tree[cur_id].access_count++;
		tree[cur_id].dirty = 1;
		if(tree[cur_id].valid && tgt_partition_size < cur_partition_size){
			tgt_partition_size = cur_partition_size;
			partition_id = cur_id;
		}
		cur_partition_size<<=1;
		cur_id<<=1;
	}

	partition_id = get_lowest_page_from_id(partition_id);
	mprotect(get_page_ptr_from_id(current_lp, partition_id), cur_partition_size*PAGE_SIZE, PROT_READ | PROT_WRITE);
}



void run_model(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int size  = 1;
	unsigned int cur_id = 0;
	unsigned int tgt_id = 0;
	unsigned int cur_partition_size;
	partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 

	for(unsigned int i = 0; i<start; i++){
		tree[i].valid = 0;
		tree[i].cost  = 0;
		tree[i].dirty = 0;
	}

	for(unsigned int i = start; i<end; i++){
		tree[i].valid = 1;
		tree[i].dirty = 0;
		tree[i].cost = estimate_cost(size, ((double)tree[i].cost) / ((double)tree[1].cost) );
	} 
	
	while(start > 0){
		size *= 2;
		for(unsigned int i = start; i<end;i+=2){
			unsigned int parent = i/2;
			tree[parent].cost = estimate_cost(size, ((double)tree[i].cost)/((double)tree[1].cost));
			if(tree[parent].cost <= tree[i].cost+tree[i+1].cost){
				tree[parent].valid = 1;
			}
		}
		end    = start;
		start /= 2; 
	}


	start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	end   = start*2;
	while(start < end){
		cur_id = start;
		cur_partition_size = 1;
		while(cur_id > 0){
			cur_id <<= 1;
			cur_partition_size <<=1;
			if(tree[cur_id].valid) tgt_id = cur_id;
		}
		tgt_id = get_lowest_page_from_id(tgt_id);
		mprotect(get_page_ptr_from_id(cur_lp, tgt_id), cur_partition_size*PAGE_SIZE, PROT_READ | PROT_WRITE);
	}
}
