#ifndef __THREAD_ALLOCATOR__
#define __THREAD_ALLOCATOR__

#include <common.h>

static inline void *thr_alloc(size_t size){ return MALLOC(size, DRAM_MEM); }
static inline void *thr_calloc(size_t nmemb, size_t size){ return CALLOC(nmemb, size, DRAM_MEM); }
static inline void *thr_aligned_alloc(size_t alignment, size_t size){ return ALIGNED_ALLOC(alignment, size, DRAM_MEM);}
static inline void  thr_free(void *ptr){ return FREE(ptr, DRAM_MEM); }
static inline int   thr_memalign_alloc(void **memptr, size_t alignment, size_t size){ return POSIX_MEMALIGN(memptr, alignment, size, DRAM_MEM);}


#endif