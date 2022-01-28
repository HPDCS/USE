#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>

#include "queue.h"
#include "core.h"
#include "lookahead.h"
#include "hpdcs_utils.h"

#if ENFORCE_LOCALITY == 1
#include "local_index/local_index.h"
#endif

//used to take locks on LPs
volatile unsigned int *lp_lock;

//event pool used by simulation as scheduler
nb_calqueue* nbcalqueue;

__thread msg_t * current_msg __attribute__ ((aligned (64)));
__thread bool safe;

__thread msg_t * new_current_msg __attribute__ ((aligned (64)));
__thread unsigned int unsafe_events;

__thread unsigned long long * lp_unsafe_set;

__thread unsigned long long * lp_unsafe_set_debug;

__thread unsigned long long * lp_locked_set;

__thread __temp_thread_pool _thr_pool  __attribute__ ((aligned (64)));

__thread list(msg_t) to_remove_local_evts_old = NULL;
__thread list(msg_t) to_remove_local_evts = NULL;
__thread list(msg_t) freed_local_evts = NULL;




void queue_init(void) {
    nbcalqueue = nb_calqueue_init(n_cores,PUB,EPB);
}

void unsafe_set_init(){
	
	if( ( lp_unsafe_set=malloc(LP_ULL_MASK_SIZE)) == NULL ){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();	
	}
    if( ( lp_unsafe_set_debug=malloc(n_prc_tot*sizeof(unsigned long long))) == NULL ){
        printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
        abort();    
    }
    if( ( lp_locked_set=malloc(n_prc_tot*sizeof(unsigned long long))) == NULL ){
        printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
        abort();    
    }
    clear_lp_unsafe_set;
    clear_lp_locked_set;
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


    msg_ptr = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
    list_node_clean_by_content(msg_ptr); //IT SHOULD NOT BE USEFUL...REMOVE?    

	//TODO: use slaballoc instead of malloc to create event descriptor
	_thr_pool.messages[_thr_pool._thr_pool_count++].father = msg_ptr;

    msg_ptr->sender_id = current_lp;
    msg_ptr->receiver_id = receiver;
    msg_ptr->timestamp = timestamp;
    msg_ptr->data_size = event_size;
    msg_ptr->type = event_type;

    memcpy(msg_ptr->data, event_content, event_size);
}

void queue_clean(){ 
    unsigned int i = 0;
    msg_t* current;
    for (; i<_thr_pool._thr_pool_count; i++)
    {
        current = _thr_pool.messages[i].father;
        if(current != NULL)
        {

            list_node_clean_by_content(current); 
            current->state = -1;
            current->data_size = tid+1;
            current->max_outgoing_ts = 0;
            list_insert_tail_by_content(freed_local_evts, current);
            _thr_pool.messages[i].father = NULL;

        }

    }
	_thr_pool._thr_pool_count = 0;
}

void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    //simtime_t max = current_msg->timestamp; //it could be done directly on current_msg->max_outgoing_ts
    
#if REPORT == 1
		clock_timer queue_op;
#endif

    for(i = 0; i < _thr_pool._thr_pool_count; i++) {

        //new_hole = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
        //list_node_clean_by_content(new_hole); //IT SHOULD NOT BE USEFUL...REMOVE?

		new_hole = _thr_pool.messages[i].father;
        _thr_pool.messages[i].father = NULL; 

        if(new_hole == NULL){
			printf("Out of memory in %s:%d", __FILE__, __LINE__);
			abort();
		}
        //memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        new_hole->fatherEpoch = LPS[current_lp]->epoch;

		new_hole->monitor = (void *)0x0;
        new_hole->state = 0;
        new_hole->epoch = 0;
        new_hole->frame = 0;
        new_hole->tie_breaker = 0;
        new_hole->max_outgoing_ts = new_hole->timestamp;

#if DEBUG==1
		if(new_hole->timestamp < current_lvt){ printf(RED("1Executing event in past!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
		//if(new_hole->timestamp != _thr_pool.messages[i].timestamp){ printf(RED("2Executing event in past!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
#endif

		if(current_msg->max_outgoing_ts < new_hole->timestamp)
			current_msg->max_outgoing_ts = new_hole->timestamp;

#if REPORT == 1
		clock_timer_start(queue_op);
#endif

        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);
#if ENFORCE_LOCALITY == 1
        nb_push(LPS[new_hole->receiver_id], new_hole);
#endif
        
#if REPORT == 1
		statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
		statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
#endif
    }

    _thr_pool._thr_pool_count = 0;
}


bool is_valid(msg_t * event){
	return  
			(event->monitor == (void *)0x5afe) 
			||
            (
                (event->state & ELIMINATED) != ELIMINATED  
                &&  (
                        event->father == NULL 
                        ||  (
                                (event->father->state  & ELIMINATED) != ELIMINATED  
                                &&   event->fatherEpoch == event->father->epoch 
                            )
                    )       
            );
}

