#ifndef __SEGMENT_H__
#define __SEGMENT_H__

#ifndef PAGE_SIZE
#define PAGE_SIZE (4ULL<<10)
#endif


#define SCALE (pdes_config.segment_shift)
#define NUM_PAGES_PER_SEGMENT ((512ULL * 512ULL)>>SCALE)

#define PER_LP_PREALLOCATED_MEMORY (NUM_PAGES_PER_SEGMENT * PAGE_SIZE ) /// This should be power of 2 multiplied by a page size. This is 1GB per LP.

#define NUM_PAGES_PER_MMAP	(NUM_PAGES_PER_SEGMENT >> 2)
#define MAX_MMAP			(NUM_PAGES_PER_MMAP * PAGE_SIZE) /// This is the maximum amount of memory that a single mmap() call is able to serve. TODO: this should be checked within configure.ac
#define NUM_MMAP			(((PER_LP_PREALLOCATED_MEMORY) / MAX_MMAP) == 0 ? 1 : ((PER_LP_PREALLOCATED_MEMORY) / MAX_MMAP))  


extern void *get_segment(unsigned int i, unsigned int numa_node, void ***pages);
extern void segment_init(void);
extern void migrate_segment(unsigned int id, int numa_node);
extern void *get_base_pointer(unsigned int gid);


#endif
