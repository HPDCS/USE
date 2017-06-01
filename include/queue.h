#ifndef __QUEUE_H
#define __QUEUE_H


#include <stdbool.h>


/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */


#define tryLock(lp)					( (lp_lock[lp*CACHE_LINE_SIZE/4]==0) && (__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], 0, tid+1)) )
#define unlock(lp)					__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], tid+1, 0) //può essere sostituita da una scrittura atomica


typedef struct __msg_t msg_t;

extern simtime_t gvt;
extern simtime_t t_btw_evts;

void queue_init(void);

void queue_insert(unsigned int receiver, double timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

double queue_pre_min(void);

unsigned int queue_pool_size(void);

void queue_register_thread(void);

int queue_pending_message_size(void);

void queue_deliver_msgs(void);

void queue_destroy(void);

void queue_clean(void);


void getMinLP(unsigned int lp);

unsigned int getMinFree();

void commit();

extern __thread msg_t * current_msg __attribute__ ((aligned (64)));
extern __thread bool  safe;
extern __thread msg_t * new_current_msg __attribute__ ((aligned (64)));
extern __thread bool  new_safe;
extern unsigned int *lp_lock;

extern void lock_init();



#endif
