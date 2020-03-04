#ifndef MEMORY_LIMIT_H
#define MEMORY_LIMIT_H

#define GIGA (1024ULL*1024ULL*1024ULL)

#ifndef MAX_ALLOCABLE_GIGAS
#error MAX_ALLOCABLE_GIGAS is not defined in makefile
#endif

#define MAX_MEMORY_ALLOCABLE (MAX_ALLOCABLE_GIGAS*GIGA) //MAX_GIGA_ALLOCABLE is defined in makefile

#if DEBUG==1
void test_memory_limit();
#endif

void set_max_memory_allocable(unsigned long max_bytes_allocable);
#endif