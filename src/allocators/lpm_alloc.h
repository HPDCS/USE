#ifndef __LP_METADATA_ALLOCATOR__
#define __LP_METADATA_ALLOCATOR__

#include <glo_alloc.h>

static inline void *lpm_alloc(size_t size){ return glo_alloc(size); }
static inline void  lpm_free(void *ptr){    return glo_free(ptr); }
static inline int   lpm_memalign_alloc(void **memptr, size_t alignment, size_t size){ return glo_memalign_alloc(memptr, alignment, size);}


#endif