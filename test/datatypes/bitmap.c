#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <datatypes/bitmap.h>
#include <glo_alloc.h>

#define ITERATIONS 100000
#define BITMAP_ENTRIES 10000

#define NO_ERROR		 		 0
#define ACTUAL_LEN_ERROR 		 1
#define NON_ZERO_BYTE_AFTER_INIT 2
#define UNVALID_GET_BIT 		 3
#define VIRTUAL_LEN_ERROR 		 4

static char messages[5][256] = {
	"none",
	"actual_len < virtual_len",
	"non zero byte after init",
	"get_bit returned an unxpected value",
	"BITMAP_ENTRIES != virtual_len"
};


// LCOV_EXCL_START

static int bitmap_test(void)
{
	unsigned int i;
	bitmap *ptr = allocate_bitmap(BITMAP_ENTRIES, DRAM_MEM);
	if(ptr->actual_len < ptr->virtual_len){ 
		return -ACTUAL_LEN_ERROR;
	}
	if(BITMAP_ENTRIES != ptr->virtual_len){ 
		return -VIRTUAL_LEN_ERROR;
	}

	for(i=0;i<ptr->actual_len/CHAR_BIT;i++)
		if(ptr->bits[i]) 
			return -NON_ZERO_BYTE_AFTER_INIT;

	char *checker = glo_alloc(BITMAP_ENTRIES, DRAM_MEM);

	i = ITERATIONS;
	while (i--) {
		unsigned int e = rand() % BITMAP_ENTRIES;
		bool v = (double)rand() / RAND_MAX > 0.5;
		if (v)
			set_bit(ptr, e);
		else
			reset_bit(ptr, e);

		checker[e] = v;
	}

	i = BITMAP_ENTRIES;
	while (i--)
		if(checker[i] != get_bit(ptr, i))
			return -UNVALID_GET_BIT;

	free(ptr);
	free(checker);

	return NO_ERROR;
}

int main(void)
{
	
	printf("Testing bitmap implementation...");
	int res =  bitmap_test();
	if(res == 0){
		printf("PASSED\n");
	}
	else
		printf("%s FAILED\n", messages[-res]);

	exit(res);
}
// LCOV_EXCL_STOP
