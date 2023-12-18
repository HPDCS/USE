#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <stdbool.h>

struct _buddy {
    size_t size;
    size_t longest[1];
};

extern bool allocator_init_for_lp(unsigned int lp);
extern bool allocator_init(void);
extern void allocator_fini(void);


#endif