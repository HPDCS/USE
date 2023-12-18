//#pragma GCC poison malloc realloc calloc aligned_alloc rsalloc rsrealloc

#ifndef __COMMON_ALLOCATORS__
#define __COMMON_ALLOCATORS__

#include <mm_nvram.h>

extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_realloc(void *, size_t);
extern void *__real_calloc(size_t, size_t);
extern int   __real_posix_memalign(void **memptr, size_t alignment, size_t size);
extern void *__real_aligned_alloc(size_t alignment, size_t size);

#define WRAP_TO_STD 0
#define WRAP_TO_REA 1
#define WRAP_TO_MKD 2

#define WRAP WRAP_TO_REA

#if WRAP == WRAP_TO_REA

#define MALLOC(x,m)   __real_malloc(x)
#define CALLOC(x,y,m) __real_calloc(x,y)
#define POSIX_MEMALIGN(x,y,z,m) __real_posix_memalign(x,y,z)
#define ALIGNED_ALLOC(x,y,m)    __real_aligned_alloc(x,y)
#define FREE(x,m)     __real_free(x)

#elif WRAP == WRAP_TO_MKD

#define MALLOC(x,m)   configurable_malloc(x)
#define CALLOC(x,y,m) configurable_calloc(x,y)
#define POSIX_MEMALIGN(x,y,z,m) configurable_posix_memalign(x,y,z)
#define ALIGNED_ALLOC(x,y,m)    configurable_aligned_alloc(x,y)
#define FREE(x,m)     configurable_free(x)

#endif

#else

#endif
