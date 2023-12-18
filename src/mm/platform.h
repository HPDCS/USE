#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stddef.h>
#include <stdlib.h>

#include <utils/error.h>

extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_realloc(void *, size_t);
extern void *__real_calloc(size_t, size_t);


static inline void *rsalloc(size_t size) {
	void *mem_block = __real_malloc(size);
	if(mem_block == NULL) {
		rootsim_error(true, "Error in Memory Allocation, aborting...");
	}
	return mem_block;
}


static inline void rsfree(void *ptr) {
	__real_free(ptr);
}


static inline void *rsrealloc(void *ptr, size_t size) {
	return __real_realloc(ptr, size);
}


static inline void *rscalloc(size_t nmemb, size_t size) {
	return __real_calloc(nmemb, size);
}

#endif