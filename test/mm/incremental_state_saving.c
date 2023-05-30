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

static char messages[5][256] = {
	"none",
	"returned an invalid page id number",
	"cannot mmap segment",
	"returned an invalid page ptr",
	"unexpected fullcheckpoint request"
};


// LCOV_EXCL_START

void **mem_areas; /// pointers to the lp segment
__thread unsigned int current_lp = 0;
simulation_configuration pdes_config;
void rsfree(void *ptr){ free(ptr); }
extern lp_iss_metadata *iss_states; /// runtime iss metadata for each lp



int get_page_idx_from_ptr(unsigned int cur_lp, void *addr);
void* get_page_ptr_from_idx(unsigned int cur_lp, unsigned int id);
void init_incremental_checkpoint_support(unsigned int num_lps);
bool is_next_ckpt_incremental(void);
void init_incremental_checkpoint_support_per_lp(unsigned int lp);




static int iss_test(void)
{
	unsigned long long segment_size = PER_LP_PREALLOCATED_MEMORY;
	unsigned int page_size = PAGE_SIZE;
	int page_id;
	mem_areas = malloc(sizeof(void*));
	void *the_address = (unsigned char *)(10LL << 39);
	void *page_ptr;
	unsigned int  counter = 0;

	pdes_config.checkpointing = INCREMENTAL_STATE_SAVING;
	pdes_config.ckpt_forced_full_period = 5;

	init_incremental_checkpoint_support(1);
	init_incremental_checkpoint_support_per_lp(0);
	
	mem_areas[0] = mmap(the_address, MAX_MMAP, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS /*|MAP_FIXED*/, 0, 0);
	if(!mem_areas[0]) return -CANNOT_MMAP;

	page_id = get_page_idx_from_ptr(current_lp, mem_areas[0]);
	if(page_id != (segment_size/page_size)) return -INVALID_PAGE_ID;

	page_ptr= get_page_ptr_from_idx(current_lp, page_id+1);
	if(page_ptr != (mem_areas[0]+PAGE_SIZE)) return -INVALID_PAGE_PTR;

	while(is_next_ckpt_incremental()) counter++;

	if(counter != pdes_config.ckpt_forced_full_period) return -WRONG_CKPT_PERIOD;


	iss_first_run_model(current_lp); /// this protects pages

	printf("address:%p page_id:%d seg_size:%lu pag_size:%u\n", mem_areas[0], page_id,PER_LP_PREALLOCATED_MEMORY, PAGE_SIZE);
	printf("page_id+1:%d,page_ptr+1:%p\n", page_id+1, page_ptr);


	printf("address:%p %p\n", mem_areas[0]+PAGE_SIZE, page_ptr);

	*((char*)mem_areas[0]) = 1;


	printf("%u\n", iss_states[0].partition_tree[1].access_count);
	printf("%u\n", iss_states[0].partition_tree[2].access_count);
	printf("%u\n", iss_states[0].partition_tree[3].access_count);
	printf("%u\n", iss_states[0].partition_tree[4].access_count);
	printf("%u\n", iss_states[0].partition_tree[page_id].access_count);

	assert(iss_states[0].partition_tree[1].access_count == 1);




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
