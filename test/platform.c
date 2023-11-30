#include <stdlib.h>

void *__real_malloc(size_t s){
	return malloc(s);
}
void __real_free(void *p){
	free(p);
}
void *__real_realloc(void *p , size_t s){
    return realloc(p,s);
}
void *__real_calloc(size_t a, size_t b){
    return calloc(a,b);
}


int   __real_posix_memalign(void **memptr, size_t alignment, size_t size){
	return posix_memalign(memptr, alignment, size);
}
void *__real_aligned_alloc(size_t alignment, size_t size){
	return aligned_alloc(alignment, size);
}