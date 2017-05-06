#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>

#include "queue.h"
#include "calqueue.h"
#include "nb_calqueue.h"
#include "core.h"
#include "lookahead.h"

//MACROs to manage lp_unsafe_set
#define add_lp_unsafe_set(lp)		( lp_unsafe_set[lp/64] |= (1 << (lp%64)) )
#define is_in_lp_unsafe_set(lp) 	( lp_unsafe_set[lp/64]  & (1 << (lp%64)) )
#define clear_lp_unsafe_set			for(unsigned int x = 0; x < (n_prc_tot/64 + 1) ; x++){lp_unsafe_set[x] = 0;}	
//MACROs to manage lock

//#define tryLock(lp)					( (lp_lock[lp*CACHE_LINE_SIZE/4]==0) && (__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], 0, tid+1)) )
//#define unlock(lp)					__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], tid+1, 0) //può essere sostituita da una scrittura atomica


//used to take locks on LPs
unsigned int *lp_lock;
//event pool used by simulation
nb_calqueue* nbcalqueue;

__thread msg_t * current_msg __attribute__ ((aligned (64)));
__thread bool safe;

__thread msg_t * new_current_msg __attribute__ ((aligned (64)));
__thread bool  new_safe;

__thread unsigned long long * lp_unsafe_set;

//unsigned long long fetchedd_evt = 0;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE]  __attribute__ ((aligned (64)));
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool  __attribute__ ((aligned (64)));




void queue_init(void) {
    nbcalqueue = nb_calqueue_init(n_cores,PUB,EPB);
	//if((lp_unsafe_set=malloc(n_prc_tot/8))==NULL){
}

void lock_init(){
		if((lp_unsafe_set=malloc(n_prc_tot/8 + 8))==NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();	
	}
	clear_lp_unsafe_set;
}

void queue_insert(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {

    msg_t *msg_ptr;

    if(_thr_pool._thr_pool_count == THR_POOL_SIZE) {
        printf("queue overflow for thread %u at time %f: inserting event %d over %d\n", tid, current_lvt, _thr_pool._thr_pool_count, THR_POOL_SIZE);
        printf("-----------------------------------------------------------------\n");
        abort();
    }
    if(event_size > MAX_DATA_SIZE) {
        printf("Error: requested a message of size %d, max allowed is %d\n", event_size, MAX_DATA_SIZE);
        abort();
    }

    msg_ptr = &_thr_pool.messages[_thr_pool._thr_pool_count++];

    msg_ptr->sender_id = current_lp;
    msg_ptr->receiver_id = receiver;
    msg_ptr->timestamp = timestamp;
    msg_ptr->data_size = event_size;
    msg_ptr->type = event_type;

    memcpy(msg_ptr->data, event_content, event_size);
    //event_content andrebbe liberato poi? Chi la libera?
    //Vedese se ha senso non copiarsi i dati ma portarsi il puntatore (perdendo la località)

}

inline unsigned int queue_pool_size(void) {
    return _thr_pool._thr_pool_count;
}

void queue_deliver_msgs(void) {

    msg_t *new_hole;
    unsigned int i;
    
    //printf("flush: pool_size = %u\n", _thr_pool._thr_pool_count);

    for(i = 0; i < _thr_pool._thr_pool_count; i++){
        new_hole = malloc(sizeof(msg_t)); //<-Si può eliminare questa malloc? Vedi queue insert
        if(new_hole == NULL){
			printf("Out of memory in %s:%d", __FILE__, __LINE__);
			abort();		
		}
        memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));

        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);
    }

    _thr_pool._thr_pool_count = 0;
    
    //printf("flush: flushed an event with type %i\n", new_hole->type);
}

//void queue_destroy(void) {
//    free(_queue.head);
//}

inline void queue_clean(void) {
    _thr_pool._thr_pool_count = 0;
}


void commit(void) {
	clock_timer queue_op;
	clock_timer_start(queue_op);
	
	queue_deliver_msgs();
	delete(nbcalqueue, current_msg->node);
	
#if DEBUG == 0
	unlock(current_lp);
#else				
	if(!unlock(current_lp))	printf("[%u] ERROR: unlock failed; previous value: %u\n", tid, lp_lock[current_lp]);
#endif
	
	if(current_lvt > gvt) gvt = current_lvt;
#if DEBUG == 1
	else if (current_lvt < gvt-LOOKAHEAD){ 
		nbc_bucket_node * node = current_msg->node;
		printf("[%u] ERROR: event coomitted out of order with GVT:%f\n", tid, gvt);
		printf("\tevent: ts:%f lp:%u resrvd:%u cpy:%u del:%u addr:%p\n", 
			node->timestamp, node->tag, node->reserved, node->copy, node->deleted, node);
	}
#endif
	//printf("[%u]I'm freeing %p\n", tid, current_msg);
	//current_msg->receiver_id = current_msg->timestamp = -1;
	free(current_msg);
	
	nbc_prune(nbcalqueue, current_lvt - LOOKAHEAD);

    statistics_post_data(tid, CLOCK_ENQUEUE, clock_timer_value(queue_op));

}

unsigned int getMinFree(){
	nbc_bucket_node * node;
	simtime_t ts, min = INFTY;
	unsigned int lp;
	
	safe = false;
	clear_lp_unsafe_set;
	    
	clock_timer queue_op;
	clock_timer_start(queue_op);
	node = getMin(nbcalqueue, -1);
    min = node->timestamp;
    
    while(node != NULL){
		lp = node->tag;
		if(!node->reserved){
			ts = node->timestamp;
			if((ts >= (min + LOOKAHEAD) || !is_in_lp_unsafe_set(lp) )){
				if(tryLock(lp)){
retry_on_replica:
					if(((unsigned long long)node->next & 1ULL)){ //da verificare se è corretto?
						if(node->replica != NULL){
							node = node->replica;
							goto retry_on_replica;
						}
#if DEBUG == 0
							unlock(lp);
#else				
							if(!unlock(lp))	printf("[%u] ERROR: unlock failed; previous value: %u\n", tid, lp_lock[lp]);
#endif

					}
					else{
						break;
					}
				}
			}
		}
		//printf("\t\t[%u]getNext: ts:%f lp:%u res:%u lk:%d\n", tid, node->timestamp, node->tag, node->reserved, lp_lock[node->tag*CACHE_LINE_SIZE/4]);
		add_lp_unsafe_set(lp);
		node = getNext(nbcalqueue, node);
    }
    
    
    statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_op));
  
    if(node == NULL){
		current_msg = NULL;
	    return 0;
    }
    
    node->reserved = true;
    current_msg = (msg_t *) node->payload;
    current_msg->node = node;

	if( ts < (min + LOOKAHEAD) && !is_in_lp_unsafe_set(lp) ){
		safe = true;
	}
    
    statistics_post_data(tid, EVENTS_FETCHED, 1);
    //statistics_post_data(tid, T_BTW_EVT, current_msg->timestamp-old_ts);

    return 1;
	
}

void getMinLP(unsigned int lp){
	nbc_bucket_node * node;
	simtime_t min;
	clock_timer queue_op;
restart:
	min = INFTY;
	
	new_safe = false;
	    
	clock_timer_start(queue_op);
	
	node = getMin(nbcalqueue, -1);
    min = node->timestamp;
    //printf("\t[%u] Min: ts:%f lp:%u resvd:%u addr:%p cp:%u\n", tid, min, node->tag, node->reserved, node, node->copy);
		
    while(node != NULL && node->tag != lp){
		//printf("\t[%u] Motherfucker, give me the lp %u at time %f %u\n", tid, lp, current_lvt, current_lp);
		node = getNext(nbcalqueue, node);
		if(node == NULL)
			goto restart;
    }
    //printf("\t[%u] Finisched %u at time %f %u\n\n", tid, lp, current_lvt, current_lp);

    
    statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_op));
  
    node->reserved = true;
    
    new_current_msg = (msg_t *) node->payload;
    new_current_msg->node = node;

	if( node->timestamp < (min + LOOKAHEAD)){
		new_safe = true;
	}
    
//    statistics_post_data(tid, EVENTS_FETCHED, 1);
}
