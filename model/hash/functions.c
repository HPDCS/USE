#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include "functions.h"



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