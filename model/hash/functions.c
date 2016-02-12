#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "functions.h"

unsigned int lock;

unsigned long long hash(const void *data, size_t size) {
	unsigned int i;
	unsigned long long hash;
	unsigned char *ptr;

	if (data == NULL) {
		return 0;
	}

	ptr = data;
	hash = 0;

	for (i = 0; i < size; i++) {
		hash = ptr[i] + (hash << 6) + (hash << 16) - hash;
	}

	return hash;
}


void state_dump(unsigned int me, const void *state, unsigned int size) {
	unsigned char *ptr = state;
	unsigned int i;
	
	while(lock == 1);
	lock = 1;

	printf("[LP%d] State at %p => ", me, ptr);

	for(i = 0; i < size; i++) {
		
		printf("%02hhx ", *(ptr+i));
		
		//if(i != 0 && (i % 8) == 0)
		//	printf(" ");
	}

	printf("\n");

	lock = 0;
	
	fflush(stdout);
}
