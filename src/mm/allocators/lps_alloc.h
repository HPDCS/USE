#ifndef __LP_STATEDATA_ALLOCATOR__
#define __LP_STATEDATA_ALLOCATOR__

#include <glo_alloc.h>

static inline void *lps_alloc(size_t size){ return glo_alloc(size); }
static inline void  lps_free(void *ptr){    return glo_free(ptr); }
static inline int   lps_memalign_alloc(void **memptr, size_t alignment, size_t size){ return glo_memalign_alloc(memptr, alignment, size);}


#endif