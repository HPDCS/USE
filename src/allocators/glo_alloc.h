#ifndef __GLOBAL_ALLOCATOR__
#define __GLOBAL_ALLOCATOR__

#include <common.h>

static inline void *glo_alloc(size_t size, memkind_const memkind){ return MALLOC(size, memkind); }
static inline void *glo_calloc(size_t nmemb, size_t size, memkind_const memkind){ return CALLOC(nmemb, size, memkind); }
static inline void *glo_aligned_alloc(size_t alignment, size_t size, memkind_const memkind){ return ALIGNED_ALLOC(alignment, size, memkind);}
static inline void  glo_free(void *ptr, memkind_const memkind){ return FREE(ptr, memkind); }
static inline int   glo_memalign_alloc(void **memptr, size_t alignment, size_t size, memkind_const memkind){ return POSIX_MEMALIGN(memptr, alignment, size, memkind);}


#endif