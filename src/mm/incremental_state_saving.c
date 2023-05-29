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




typedef struct __per_lp_iss_metadata{

}lp_iss_metadata;




inline partition_log* take_partition_log(char *ptr, size_t size, simtime_t ts){
	partition_log *new_log = (partition_log*) rsalloc(sizeof(partition_log) + size);
	new_log->size = size;
	new_log->next = NULL;
	new_log->ts   = ts;
	new_log->addr = ptr;
	new_log->log  = ((char*)new_log) + sizeof(partition_log);
	memcpy(new_log->log, ptr, size); 
}


inline void destroy_partition_log(partition_log *log_str){
	rsfree(log_str);
}

inline void restore_partition_log(partition_log *log_str){
	memcpy(new_log->addr, new_log->log, size); 	
}


inline unsigned int get_partition_id_from_address(unsigned int cur_lp, void *addr){
// take the bitmap
// index of page included 
// TODO
}


inline unsigned int get_page_id_from_address(unsigned int cur_lp, void *addr){
// take the base of the segment mem_areas[cur_lp]
// pg_addr = addr & (~ (PAGE_SIZE-1))
// byte_off= pg_addr - base;
// page_id = byte_off/PAGE_SIZE
}

inline unsigned long long *get_tree_counters(unsigned int lp){
// TODO
	return NULL;
}

inline unsigned int get_first_leaf_index(){
// TODO
	return 0;
}

inline double estimate_cost(model_t *model, size_t size, double probability){
	return size*model->mprotect_cost_per_page + probability*model->log_cost_per_page;
}

inline void update_access_counters(unsigned int page_id, unsigned int lp){
	unsigned long long *tree_counters = get_tree_counters(lp);
	unsigned int cur_id = page_id;
	while(cur_id > 0){
		tree_counters[cur_id]++;
		cur_id<<=1;
	}
}

inline void dirty(void* addr, size_t size){
	unsigned int partition_id = get_partition_id_from_address(current_lp, addr);
	unsigned int page_id      = get_page_id_from_address(current_lp, addr);
	update_access_counters(page_id);
	// TODO
}

inline unsigned int get_max_order(){
// TODO
	return 0;
}

inline unsigned int get_num_nodes_at_order(unsigned int order){
// TODO
	return 0;
}


inline void run_model(unsigned int cur_lp){
	unsigned long long *tree_counters = get_tree_counters(lp);
	double *costs_map = get_costs_map(lp);
	bitmap *partion_bitmap = get_partitions_bitmap(lp);
	
	unsigned cur_order = 0, start = get_first_leaf_index(), end = 1>>get_max_order();
	reset_bitmap(partion_bitmap);

	for(unsigned int i = start; i<end;i++){
		set_bit(partion_bitmap, i);			
		costs_map[i] = estimate_cost(model, 1<<cur_order, ((double)tree_counters[i])((double)tree_counters[1]));
	} 
	
	while(cur_order <= get_max_order()){
		for(unsigned int i = start; i<end;i+=2){
			unsigned int parent = i>>1;
			costs_map[parent] = estimate_cost(model, 2<<cur_order, ((double)tree_counters[parent])((double)tree_counters[parent]));
			if(costs_map[parent]<= costs_map[i]+costs_map[i+1]){
				set_bit(bitmap, parent);
			}
		} 
	}
}