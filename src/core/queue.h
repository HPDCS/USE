#ifndef __QUEUE_H
#define __QUEUE_H

#include <stdbool.h>
#include "nb_calqueue.h"

#include <lp/lp_lock.h>

typedef struct __msg_t msg_t;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	unsigned int _thr_pool_size; 
	msg_t messages[]  __attribute__ ((aligned (64)));
} __temp_thread_pool;

void queue_init(void);

void queue_insert(unsigned int receiver, double timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

//unsigned int queue_pool_size(void);

void queue_deliver_msgs(void);

//void queue_destroy(void);

void queue_clean(void);
void queue_init_per_thread(void);

extern void unsafe_set_init();

extern bool is_valid(msg_t * event);

extern nb_calqueue* nbcalqueue;

extern __thread msg_t * current_msg __attribute__ ((aligned (64)));
extern __thread msg_t * new_current_msg __attribute__ ((aligned (64)));
extern __thread bool  safe;

extern __thread __temp_thread_pool *_thr_pool;
extern __thread unsigned long long * lp_unsafe_set;
extern __thread unsigned long long * lp_unsafe_set_debug;
extern __thread unsigned long long * lp_locked_set;
extern __thread unsigned int unsafe_events;
extern __thread list(msg_t) to_remove_local_evts;
extern __thread list(msg_t) to_remove_local_evts_old;
extern __thread list(msg_t) freed_local_evts;


//#define add_lp_unsafe_set(lp)		( lp_unsafe_set[lp/64] |= (1ULL << (lp%64)) )
#define add_lp_unsafe_set(lp) 		(lp_unsafe_set_debug[lp]=1ULL)
//#define is_in_lp_unsafe_set(lp) 	( lp_unsafe_set[lp/64]  & (1ULL << (lp%64)) )
#define is_in_lp_unsafe_set(lp)		( lp_unsafe_set_debug[lp]==1ULL )
//#define clear_lp_unsafe_set		{unsigned int x; for(x = 0; x < (pdes_config.nprocesses/64 + 1) ; x++){lp_unsafe_set[x] = 0;}
#define clear_lp_unsafe_set         {unsigned int x; for(x = 0; x < (pdes_config.nprocesses) ; x++){lp_unsafe_set_debug[x] = 0;}}


#define clear_lp_locked_set			{ unsigned int x; for(x = 0; x < (pdes_config.nprocesses/64 + 1) ; x++){lp_locked_set[x] = 0;}}
#define add_lp_locked_set(lp) 		( lp_locked_set[lp/64] |= (1ULL << (lp%64)) )
#define is_in_lp_locked_set(lp)		( lp_locked_set[lp/64]  & (1ULL << (lp%64)) )

#define queue_pool_size _thr_pool._thr_pool_count;

#define THR_POOL_SIZE		128


#endif
