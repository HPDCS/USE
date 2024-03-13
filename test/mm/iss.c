#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <mm.h>
#include <incremental_state_saving.h>

#define ITERATIONS 100000
#define BITMAP_ENTRIES 10000

#define NO_ERROR		 		 0
#define INVALID_PAGE_ID 		 1
#define CANNOT_MMAP				 2
#define INVALID_PAGE_PTR 		 3
#define WRONG_CKPT_PERIOD 		 4
#define INVALID_SEGID	 		 5
#define NO_USER_DATA			 6

static char messages[7][256] = {
	"none",
	"returned an invalid page id number",
	"cannot mmap segment",
	"returned an invalid page ptr",
	"unexpected fullcheckpoint request",
	"invalid segment id",
	"user data from kernel are null",
};


// LCOV_EXCL_START

void **mem_areas; /// pointers to the lp segment
__thread unsigned int current_lp = 0;
 __thread unsigned int tid = 0;
simulation_configuration pdes_config;
void rsfree(void *ptr){ free(ptr); }
void* rsalloc(size_t len){ return malloc(len); }
extern lp_iss_metadata *iss_states; /// runtime iss metadata for each lp


int get_page_idx_from_ptr(unsigned int cur_lp, void *addr);
void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id);
void init_incremental_checkpointing_support(unsigned int threads, unsigned int lps);
bool is_next_ckpt_incremental(void);
void init_incremental_checkpoint_support_per_lp(unsigned int lp);
void iss_update_model(unsigned int cur_lp);
void iss_run_model(unsigned int cur_lp);
void iss_first_run_model(unsigned int cur_lp);


int guard_memory(unsigned int lid, unsigned long size);
int unguard_memory(unsigned int lid, unsigned long size);
int flush(unsigned int lid, unsigned long size);


float estimate_cost(size_t size, float probability);



static int iss_test(void)
{
	unsigned long long segment_size = PER_LP_PREALLOCATED_MEMORY;
	unsigned int page_size = PAGE_SIZE;
	int page_id; 
	mem_areas = malloc(sizeof(void*));
	void *the_address = (unsigned char *)(10LL << 39);
	void *page_ptr;
	unsigned int  counter = 0;
	unsigned long segid;
	pdes_config.checkpointing = INCREMENTAL_STATE_SAVING;
	pdes_config.ckpt_forced_full_period = 5;
	pdes_config.iss_enabled_mprotection = 1;
	pdes_config.iss_signal_mprotect 	= 0;

	open_tracker_device("/dev/tracker", (O_RDONLY | O_NONBLOCK));

	init_incremental_checkpointing_support(1, 1);

	
	mem_areas[0] = mmap(the_address, MAX_MMAP, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS /*|MAP_FIXED*/, 0, 0);
	if(!mem_areas[0]) return -CANNOT_MMAP;

	page_id = get_page_idx_from_ptr(current_lp, mem_areas[0]);
	if(page_id != (int)(segment_size/page_size)) return -INVALID_PAGE_ID;

	segid = SEGID(mem_areas[0], mem_areas[0], NUM_PAGES_PER_SEGMENT);
	if (segid != 0) return -INVALID_SEGID;

	page_ptr= get_page_ptr_from_idx(current_lp, page_id+1);
	if(page_ptr != (mem_areas[0]+PAGE_SIZE)) return -INVALID_PAGE_PTR;

	init_incremental_checkpoint_support_per_lp(0);

	memset(mem_areas[current_lp], 0, MAX_MMAP);


	ioctl(device_fd, TRACKER_DUMP);


	while(is_next_ckpt_incremental()) counter++;

	if(counter != pdes_config.ckpt_forced_full_period) return -WRONG_CKPT_PERIOD;

	//partition_node_tree_t *tree = &iss_states[0].partition_tree[0];

	
	printf("address:%p page_id:%d seg_size:%llu pag_size:%llu segment_id %lu\n", mem_areas[0], page_id,PER_LP_PREALLOCATED_MEMORY, PAGE_SIZE, segid);
	printf("page_id+1:%d,page_ptr+1:%p\n", page_id+1, page_ptr);
	printf("address:%p %p\n", mem_areas[0]+PAGE_SIZE, page_ptr);
	

/*
	#define TREE 19

	for(int i=0;i<TREE;i++){
		page_id &= ~1ULL;
		printf("%u: cost=%f valid=%u access:%llu\n", page_id, iss_states[0].partition_tree[page_id].cost, iss_states[0].partition_tree[page_id].valid, iss_states[0].partition_tree[page_id].access_count);
		page_id++;
		printf("%u: cost=%f valid=%u access:%llu\n", page_id, iss_states[0].partition_tree[page_id].cost, iss_states[0].partition_tree[page_id].valid, iss_states[0].partition_tree[page_id].access_count);
		printf("\n");
		page_id >>=1;
	}
	page_id = 1;
	page_id <<= TREE-1;
		printf("\n");
		printf("\n");


	
	page_id += 1024;
	printf("page_id %u\n", page_id);
	for(int i=0;i<TREE;i++){
		page_id &= ~1ULL;
		printf("%u: cost=%f valid=%u access:%llu\n", page_id, iss_states[0].partition_tree[page_id].cost, iss_states[0].partition_tree[page_id].valid, iss_states[0].partition_tree[page_id].access_count);
		page_id++;
		printf("%u: cost=%f valid=%u access:%llu\n", page_id, iss_states[0].partition_tree[page_id].cost, iss_states[0].partition_tree[page_id].valid, iss_states[0].partition_tree[page_id].access_count);
		printf("\n");
		page_id >>=1;
	}
	page_id = 1;
	page_id <<=TREE-1;
*/



	iss_first_run_model(current_lp); /// this protects pages
	guard_memory(current_lp, PAGE_SIZE);
	*((char*)mem_areas[0]) = 1;

	/*assert(iss_states[0].partition_tree[1].access_count == 0);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 0);
	assert(iss_states[0].partition_tree[page_id].valid == 1);
	assert(iss_states[0].current_incremental_log_size == PAGE_SIZE);*/

	*((char*)mem_areas[0]) = 1;
	/*assert(iss_states[0].partition_tree[1].access_count == 0);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 0);
	assert(iss_states[0].partition_tree[page_id].valid == 1);
	assert(iss_states[0].current_incremental_log_size == PAGE_SIZE);*/

	unguard_memory(current_lp, PAGE_SIZE);
	guard_memory(current_lp, PAGE_SIZE);

	*((char*)mem_areas[0]) = 1;
	/*assert(iss_states[0].partition_tree[1].access_count == 0);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 0);
	assert(iss_states[0].partition_tree[page_id].valid == 1);*/

	tracking_data *data2 = get_fault_info(current_lp);
	if (data2 == NULL) return -NO_USER_DATA;

	iss_update_model(current_lp);

	*((char*)mem_areas[0]) = 1;
	/*assert(iss_states[0].partition_tree[1].access_count == 1);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 1);
	assert(iss_states[0].partition_tree[page_id].valid == 1);*/

	guard_memory(current_lp, PAGE_SIZE);

	/*assert(iss_states[0].partition_tree[1].access_count == 1);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 1);
	assert(iss_states[0].partition_tree[page_id].valid == 1);*/

	*((char*)mem_areas[0]) = 1;

	/*assert(iss_states[0].partition_tree[1].access_count == 1);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 1);
	assert(iss_states[0].partition_tree[page_id].valid == 1);*/


	iss_update_model(current_lp);

	/*assert(iss_states[0].partition_tree[1].access_count == 2);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id].access_count == 2);
	assert(iss_states[0].partition_tree[page_id].valid == 1);*/


	guard_memory(current_lp, PAGE_SIZE);
	*(((char*)mem_areas[0])+PAGE_SIZE) = 1;
	iss_update_model(current_lp);
	guard_memory(current_lp, PAGE_SIZE);


	/*assert(iss_states[0].partition_tree[1].access_count == 3);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 1);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);*/
	

	*(((char*)mem_areas[0])+PAGE_SIZE) = 1;
	iss_update_model(current_lp);


	/*assert(iss_states[0].partition_tree[1].access_count == 4);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 2);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);*/


	*(((char*)mem_areas[0])+PAGE_SIZE) = 1;
	iss_update_model(current_lp);

	/*assert(iss_states[0].partition_tree[1].access_count == 4);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 2);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);*/

	iss_update_model(current_lp);
	/*iss_states[current_lp].current_incremental_log_size = 0; // clean log size
	assert(iss_states[0].partition_tree[1].access_count == 4);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 2);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);*/


	guard_memory(current_lp, 2*PAGE_SIZE);
	*(((char*)mem_areas[0])+PAGE_SIZE) = 2;
	iss_update_model(current_lp);
	iss_states[current_lp].current_incremental_log_size = 0; // clean log size

	guard_memory(current_lp, 2*PAGE_SIZE);
	*(((char*)mem_areas[0])+PAGE_SIZE) = 2;
	iss_update_model(current_lp);
	iss_states[current_lp].current_incremental_log_size = 0; // clean log size

	guard_memory(current_lp, 2*PAGE_SIZE);
	*(((char*)mem_areas[0])+PAGE_SIZE) = 2;

	//partition_log *log = log_incremental(current_lp, 0.0);



	iss_update_model(current_lp);
	
	/*assert(iss_states[0].partition_tree[1].access_count == 7);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 5);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);
	assert(*(((char*)mem_areas[0])+PAGE_SIZE) == 2);*/

	*(((char*)mem_areas[0])+PAGE_SIZE) = 3;
	assert(*(((char*)mem_areas[0])+PAGE_SIZE) == 3);
	//log_incremental_restore(log);
	//log_incremental_destroy_chain(log);

	/*assert(iss_states[0].partition_tree[1].access_count == 7);
	assert(iss_states[0].partition_tree[1].valid == 0);
	assert(iss_states[0].partition_tree[page_id+1].access_count == 5);
	assert(iss_states[0].partition_tree[page_id+1].valid == 1);
	assert(*(((char*)mem_areas[0])+PAGE_SIZE) == 2);*/

	tracking_data *data = get_fault_info(current_lp);
	if (data == NULL) return -NO_USER_DATA;
	
	unsigned long len = data->len_buf;
	unsigned long *buff = malloc(sizeof(unsigned long) * len);

	buff = data->buff_addresses;
		

	for (int i=0; i < len; i++) {
		fprintf(stderr, "[i %d] element %lu\n", i, buff[i]);
	}





	return NO_ERROR;
}

int main(void)
{
	
	printf("Testing iss implementation:\n");
	int res =  iss_test();
	if(res == 0){
		printf("PASSED\n");
	}
	else
		printf("%s FAILED\n", messages[-res]);

	exit(res);
}
// LCOV_EXCL_STOP
