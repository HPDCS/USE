#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>

#include "queue.h"
#include "core.h"
#include "lookahead.h"

//used to take locks on LPs
unsigned int *lp_lock;
//event pool used by simulation
nb_calqueue* nbcalqueue;

__thread msg_t * current_msg __attribute__ ((aligned (64)));
__thread bool safe;

__thread msg_t * new_current_msg __attribute__ ((aligned (64)));
__thread unsigned int unsafe_events;

__thread unsigned long long * lp_unsafe_set;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE]  __attribute__ ((aligned (64)));
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool  __attribute__ ((aligned (64)));




void queue_init(void) {
    nbcalqueue = nb_calqueue_init(n_cores,PUB,EPB);
}

void lock_init(){
	if( ( lp_unsafe_set=malloc(LP_ULL_MASK_SIZE)) == NULL ){
	//if((lp_unsafe_set=malloc(n_prc_tot/8 + 8))==NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();	
	}
	clear_lp_unsafe_set;
}

void queue_insert(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {

    msg_t *msg_ptr;

    if(_thr_pool._thr_pool_count == THR_POOL_SIZE) {
        printf("queue overflow for thread %u at time %f: inserting event %d over %d\n", tid, current_lvt, _thr_pool._thr_pool_count, THR_POOL_SIZE);
        abort();
    }
    if(event_size > MAX_DATA_SIZE) {
        printf("Error: requested a message of size %d, max allowed is %d\n", event_size, MAX_DATA_SIZE);
        abort();
    }

	//TODO: slaballoc al posto della malloc per creare il desrittore dell'evento
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
    simtime_t max = 0; //potrebbe essere fatto direttamente su current_msg->max_outgoing_ts

    for(i = 0; i < _thr_pool._thr_pool_count; i++){

        //new_hole = malloc(sizeof(msg_t)); //<-Si può eliminare questa malloc? Vedi queue insert
        new_hole = list_allocate_node_buffer(current_lp, sizeof(msg_t));
        if(new_hole == NULL){
			printf("Out of memory in %s:%d", __FILE__, __LINE__);
			abort();		
		}
        memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        new_hole->fatherEpoch = LPS[current_lp]->epoch;

		if(max < new_hole->timestamp)
			max = new_hole->timestamp;

#if REPORT == 1
		clock_timer queue_op;
		clock_timer_start(queue_op);
#endif
        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);

#if REPORT == 1
		statistics_post_data(tid, CLOCK_ENQUEUE, clock_timer_value(queue_op));
		statistics_post_data(tid, EVENTS_FLUSHED, 1);
#endif
    }
    current_msg->max_outgoing_ts = max;
    _thr_pool._thr_pool_count = 0;
}

inline void queue_clean(void) {
    _thr_pool._thr_pool_count = 0;
}

void commit(void) {
	queue_deliver_msgs();
#if REPORT == 1
	clock_timer queue_op;
	clock_timer_start(queue_op);
#endif
	delete(nbcalqueue, current_msg->node);
#if REPORT == 1
	statistics_post_data(tid, CLOCK_DELETE, clock_timer_value(queue_op));
#endif
	
#if DEBUG == 0
	unlock(current_lp);
#else				
	if(!unlock(current_lp))	printf("[%u] ERROR: unlock failed; previous value: %u\n", tid, lp_lock[current_lp]);
#endif

#if DEBUG == 1
	simtime_t old_gvt = gvt; 
	if(current_lvt > old_gvt) 
		__sync_bool_compare_and_swap((unsigned long long*)&gvt, (unsigned long long)old_gvt, (unsigned long long)current_lvt);
	else if (current_lvt < old_gvt-LOOKAHEAD){ 
		nbc_bucket_node * node = current_msg->node;
		printf("[%u] ERROR: event coomitted out of order with GVT:%f\n", tid, gvt);
		printf("\tevent: ts:%f lp:%u resrvd:%u cpy:%u del:%u addr:%p\n", 
			node->timestamp, node->tag, node->reserved, node->copy, node->deleted, node);
	}
#endif
	free(current_msg);
#if REPORT == 1
	clock_timer_start(queue_op);
#endif
	//nbc_prune(current_lvt - LOOKAHEAD);
	nbc_prune();
#if REPORT == 1
	statistics_post_data(tid, CLOCK_PRUNE, clock_timer_value(queue_op));
	statistics_post_data(tid, PRUNE_COUNTER, 1);
#endif
}


unsigned int getMinFree(){
	nbc_bucket_node * node;
	simtime_t ts, min = INFTY;
	unsigned int lp;
	table *h;

#if REPORT == 1
	clock_timer queue_op;
	clock_timer_start(queue_op);
#endif
    if((node = getMin(nbcalqueue, &h)) == NULL)
		return 0;
	safe = false;
	clear_lp_unsafe_set;
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
		node = getNext(node, h);
    }
    
    if(node == NULL){
		current_msg = NULL;
	    return 0;
    }
    
    node->reserved = true;
    current_msg = (msg_t *) node->payload;
    current_msg->node = node;

	if( ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD==0 && (ts == min))) && !is_in_lp_unsafe_set(lp) )
		safe = true;
    
#if REPORT == 1
	statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_op));
	statistics_post_data(tid, EVENTS_FETCHED, 1);
//	statistics_post_data(tid, T_BTW_EVT, current_msg->timestamp-old_ts); // TODO : possiamo usarlo per la resize? 
#endif
    return 1;
	
}

void getMinLP(unsigned int lp){
	nbc_bucket_node * node;
	simtime_t min;
	table *h;
	
#if REPORT == 1
	clock_timer queue_op;
	clock_timer_start(queue_op);
#endif

restart:
	min = INFTY;
	safe = false;
	    
	node = getMin(nbcalqueue, &h);
    min = node->timestamp;
   	
    while(node != NULL && node->tag != lp){
		node = getNext( node, h);
		if(node == NULL)
			goto restart;
    }
    //printf("\t[%u] Finisched %u at time %f %u\n\n", tid, lp, current_lvt, current_lp);

    node->reserved = true;
    new_current_msg = (msg_t *) node->payload;
    new_current_msg->node = node;

	if( (node->timestamp < (min + LOOKAHEAD)) || (LOOKAHEAD==0 && (node->timestamp == min)) ){
		safe = true;
	}
#if REPORT == 1
	statistics_post_data(tid, CLOCK_DEQ_LP, clock_timer_value(queue_op));
#endif
}

bool is_valid(msg_t * event){
	printf("IS_VALID TODO\n");
	return true;
}

