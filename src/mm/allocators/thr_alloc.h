#ifndef __THREAD_ALLOCATOR__
#define __THREAD_ALLOCATOR__

#include <common.h>

static inline void *thr_alloc(size_t size){ return MALLOC(size); }
static inline void *thr_calloc(size_t nmemb, size_t size){ return CALLOC(nmemb, size); }
static inline void *thr_aligned_alloc(size_t alignment, size_t size){ return ALIGNED_ALLOC(alignment, size);}
static inline void  thr_free(void *ptr){ return FREE(ptr); }
static inline int   thr_memalign_alloc(void **memptr, size_t alignment, size_t size){ return POSIX_MEMALIGN(memptr, alignment, size);}


#endif