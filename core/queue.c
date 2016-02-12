
/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Estrazione Thread-safe (non lock-free)
 * Inserimenti avvengono in transazioni
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>

#include "queue.h"
#include "calqueue.h"
#include "core.h"
#include "lookahead.h"

__thread msg_t current_msg __attribute__ ((aligned (64)));

unsigned long long fetched_evt = 0;

/* commit horizon */
simtime_t gvt = 0;
/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE]  __attribute__ ((aligned (64)));
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool  __attribute__ ((aligned (64)));

int queue_lock = 0;

void queue_init(void) {
    calqueue_init();
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
}

unsigned int queue_pool_size(void) {
    return _thr_pool._thr_pool_count;
}

void queue_deliver_msgs(void) {

    msg_t *new_hole;
    unsigned int i;

    for(i = 0; i < _thr_pool._thr_pool_count; i++){
        new_hole = malloc(sizeof(msg_t));
        if(new_hole == NULL){
			printf("Out of memory in %s:%d", __FILE__, __LINE__);
			abort();
		}
        memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
        calqueue_put(new_hole->timestamp, new_hole);
    }

    _thr_pool._thr_pool_count = 0;
}

int queue_min(void) {

    while(__sync_lock_test_and_set(&queue_lock, 1))
        while(queue_lock);

    msg_t *node_ret;
    node_ret = calqueue_get();

    if(node_ret == NULL){
        __sync_lock_release(&queue_lock);
        return 0;
    }

    memcpy(&current_msg, node_ret, sizeof(msg_t));
    free(node_ret);

    execution_time(current_msg.timestamp, current_msg.receiver_id);

    statistics_post_data(tid, EVENTS_FETCHED, 1);
    statistics_post_data(tid, T_BTW_EVT, current_msg.timestamp);

    __sync_lock_release(&queue_lock);

    return 1;

}


int fetch(void) {
    return queue_min();
}

void queue_clean(void) {
    _thr_pool._thr_pool_count = 0;
}

void flush(void) {
    while(__sync_lock_test_and_set(&queue_lock, 1))
        while(queue_lock);

	if(current_lvt>gvt) gvt = current_lvt;
	else if(current_lvt+LOOKAHEAD < gvt)  printf("GVT: WTF!!! old=%f new=%f\n", gvt, current_lvt);

	queue_deliver_msgs();

    __sync_lock_release(&queue_lock);

    /* This part is useful only if the lookahead is greater than 0
	simtime_t tmp_gvt;
	void * ptr_gvt = &gvt;
ret:
    tmp_gvt = gvt;
    if(tmp_gvt < current_lvt){
		if(__sync_val_compare_and_swap(&gvt, tmp_gvt, current_lvt) == tmp_gvt)
			return;
		else
			goto ret;
	}
	*/
}
