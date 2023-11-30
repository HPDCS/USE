//#pragma GCC poison malloc realloc calloc aligned_alloc rsalloc rsrealloc

#ifndef __COMMON_ALLOCATORS__
#define __COMMON_ALLOCATORS__


extern void *__real_malloc(size_t);
extern void __real_free(void *);
extern void *__real_realloc(void *, size_t);
extern void *__real_calloc(size_t, size_t);
extern int   __real_posix_memalign(void **memptr, size_t alignment, size_t size);
extern void *__real_aligned_alloc(size_t alignment, size_t size);

#define WRAP 1

#if WRAP

#define MALLOC __real_malloc
#define CALLOC __real_calloc
#define POSIX_MEMALIGN __real_posix_memalign
#define ALIGNED_ALLOC __real_aligned_alloc
#define FREE __real_free

#else

#define MALLOC malloc
#define CALLOC calloc
#define POSIX_MEMALIGN posix_memalign
#define ALIGNED_ALLOC aligned_alloc
#define FREE free
#endif

#endif