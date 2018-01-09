#ifndef __QUEUE_H
#define __QUEUE_H

#include <stdbool.h>
#include "nb_calqueue.h"

#define tryLock(lp)					( (lp_lock[lp*CACHE_LINE_SIZE/4]==0) && (__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], 0, tid+1)) )
#define unlock(lp)					__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], tid+1, 0) //può essere sostituita da una scrittura atomica

#define add_lp_unsafe_set(lp)		( lp_unsafe_set[lp/64] |= (1ULL << (lp%64)) )
#define is_in_lp_unsafe_set(lp) 	( lp_unsafe_set[lp/64]  & (1ULL << (lp%64)) )
#define clear_lp_unsafe_set			unsigned int x; for(x = 0; x < (n_prc_tot/64 + 1) ; x++){lp_unsafe_set[x] = 0;}


typedef struct __msg_t msg_t;

void queue_init(void);

void queue_insert(unsigned int receiver, double timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

unsigned int queue_pool_size(void);

void queue_deliver_msgs(void);

void queue_destroy(void);

void queue_clean(void);

extern void unsafe_set_init();

void getMinLP(unsigned int lp);
unsigned int getMinFree();
void commit();

extern nb_calqueue* nbcalqueue;

extern __thread msg_t * current_msg __attribute__ ((aligned (64)));
extern __thread msg_t * new_current_msg __attribute__ ((aligned (64)));
extern __thread bool  safe;

extern unsigned int *lp_lock;
extern __thread unsigned long long * lp_unsafe_set;
extern __thread unsigned int unsafe_events;

#endif
