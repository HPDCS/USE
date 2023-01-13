#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>


#include <datatypes/list.h>
#include <core.h>

#define ITERATIONS 100000
#define BITMAP_ENTRIES 10000

#define NO_ERROR		 		 0
#define NOT_EMPTY		 		 1
#define EMPTY		 		 	 2
#define UNVALID_GET_BIT 		 3
#define VIRTUAL_LEN_ERROR 		 4

static char messages[5][256] = {
	"none",
	"list should be empty",
	"list should not be empty",
	"non zero byte after init",
	"BITMAP_ENTRIES != virtual_len"
};

simulation_configuration rootsim_config;
size_t node_size_msg_t;


void *rsalloc(size_t size) { return malloc(size); }
void rsfree(void *ptr) { free(ptr);}
extern void *umalloc(int lid, size_t size);




static int list_test(void)
{
	unsigned int i=0;
	list(int)	queue = new_list(GENERIC_LIST, int);

	if(!list_empty(queue)) return -NOT_EMPTY;

	list_insert_head(GENERIC_LIST, queue, &i);	i++;
	if(list_empty(queue)) return -EMPTY;

	list_insert_head(GENERIC_LIST, queue, &i); 	i++;
	if(list_empty(queue)) return -EMPTY;

	list_pop(GENERIC_LIST, queue);
	if(list_empty(queue)) return -EMPTY;

	list_pop(GENERIC_LIST, queue);
	if(!list_empty(queue)) return -NOT_EMPTY;


	list_insert_tail(GENERIC_LIST, queue, &i); 	i++;
	if(list_empty(queue)) return -EMPTY;

	list_insert_tail(GENERIC_LIST, queue, &i); 	i++;
	if(list_empty(queue)) return -EMPTY;

	list_pop(GENERIC_LIST, queue);
	if(list_empty(queue)) return -EMPTY;

	list_pop(GENERIC_LIST, queue);
	if(!list_empty(queue)) return -NOT_EMPTY;

	return NO_ERROR;
}

int main(void)
{
	
	printf("Testing list implementation...");
	int res =  list_test();
	if(res == 0){
		printf("PASSED\n");
	}
	else
		printf("%s FAILED\n", messages[-res]);

	exit(res);
}
