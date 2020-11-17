#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>
#include <unistd.h>

#include "queue.h"
#include "core.h"
#include "lookahead.h"
#include "hpdcs_utils.h"
#include <prints.h>

#if DEBUG==1
#include <checks.h>
#endif

#if POSTING==1
#include <posting.h>
#endif

#if IPI_SUPPORT==1
#include <ipi.h>
#endif

#if CONSTANT_CHILD_INVALIDATION==1
#include <atomic_epoch_and_ts.h>
#endif

#if LONG_JMP==1
#include <jmp.h>
extern __thread cntx_buf cntx_loop;
#endif

#if PREEMPT_COUNTER==1
#include <preempt_counter.h>
#endif
#if HANDLE_INTERRUPT_WITH_CHECK==1
#include <handle_interrupt_with_check.h>
#endif

#if HANDLE_INTERRUPT==1
#include <handle_interrupt.h>

//override memcpy to interrupt library function "memcpy"
#define memcpy(d, s, n) ({ \
    void *res; \
    asm volatile ( \
        "cld\n" \
        "rep\n" \
        "movsb\n" \
        : "=r" (res) \
        : "0" (d), "c" (n), "S" (s), "D" (d) \
        : "cc", "memory" \
    ); \
    res; \
})
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

__thread __temp_thread_pool * _thr_pool_ptr  __attribute__ ((aligned (64)));
#define _thr_pool (*_thr_pool_ptr)

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

    #if HANDLE_INTERRUPT_WITH_CHECK==1
    enter_in_unpreemptable_zone();
    #endif

    //branch "if" not present in original version because _thr_pool.messages[_thr_pool._thr_pool_count].father==NULL 
    //because previous call are queue_clean and queue_deliver_msgs not interruptible
    if(_thr_pool.messages[_thr_pool._thr_pool_count].father!=NULL){//re-use event
        msg_ptr=_thr_pool.messages[_thr_pool._thr_pool_count++].father;//get message
        list_node_clean_by_content(msg_ptr); //NON DOVREBBE SERVIRE
        #if REPORT==1 && HANDLE_INTERRUPT==1
        statistics_post_lp_data(current_lp,STAT_EVENT_NOT_FLUSHED_LP,1);
        #endif
    }
    else{//extract event from free_list and connect to thread_pool
        msg_ptr = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
        list_node_clean_by_content(msg_ptr); //NON DOVREBBE SERVIRE    
        //TODO: slaballoc al posto della malloc per creare il descrittore dell'evento
        _thr_pool.messages[_thr_pool._thr_pool_count++].father = msg_ptr;
        
    }
    
    #if HANDLE_INTERRUPT_WITH_CHECK==1
    exit_from_unpreemptable_zone(default_handler,INVALID);
    #endif

    msg_ptr->sender_id = current_lp;
    msg_ptr->receiver_id = receiver;
    msg_ptr->timestamp = timestamp;
    msg_ptr->data_size = event_size;
    msg_ptr->type = event_type;

#if HANDLE_INTERRUPT==1
    msg_ptr->processing_info.evt_start_time = 0;//event starting time initialization
#endif

    memcpy(msg_ptr->data, event_content, event_size);
}

#if HANDLE_INTERRUPT==1
void queue_clean(){//this function is interruptible,because this is the first function called before any ProcessEvent
    _thr_pool._thr_pool_count = 0;
}

#else//original version of queue_clean
//TODO modify queue_clean queue_insert queue_deliver_msg in order to follows implementation present in HANDLE_INTERRUPT macro,with cleaning in o(1)
//and buffer reusage in queue_insert
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
#endif

#if POSTING==1
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    #if DEBUG==1 //not present in original version
    unsigned int father_lp_idx=(unsigned int)INVALID_LP_IDX;//invalid lp_idx
    #endif
    bool flagged=false,posted=false;//if flagged is true,then event is flagged POSTED_VALID,if posted is true then event is posted successfully
#if REPORT == 1
        clock_timer queue_op;
#endif
#if DEBUG==1//not present in original version
        check_queue_deliver_msgs();
#endif

    for(i = 0; i < _thr_pool._thr_pool_count; i++) {

        new_hole = _thr_pool.messages[i].father;
        if(new_hole == NULL){
            printf("Out of memory in %s:%d", __FILE__, __LINE__);
            abort();
        }
        #if DEBUG==1//not present in original version
        if(father_lp_idx==(unsigned int)INVALID_LP_IDX){//first time set father_lp_idx with child_lp_idx
            father_lp_idx=new_hole->sender_id;
        }
        if(father_lp_idx!=new_hole->sender_id){//check if father_lp_idx is the same for each child
            printf("queue_deliver_msgs is flushing events with different father->lp_idx\n");
            gdb_abort;
        }
        #endif
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        #if CONSTANT_CHILD_INVALIDATION==1
        new_hole->fatherEpoch = get_epoch_of_LP(current_lp);
        #else
        new_hole->fatherEpoch = LPS[current_lp]->epoch;
        #endif

        new_hole->monitor = (void *)0x0;
        new_hole->state = NEW_EVT;
        new_hole->epoch = 0;
        new_hole->frame = 0;
        new_hole->tie_breaker = 0;
        new_hole->max_outgoing_ts = new_hole->timestamp;
        new_hole->posted=NEVER_POSTED;
        new_hole->collectionable=false;
#if DEBUG==1
        if(new_hole->timestamp < current_lvt){
            printf("Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f",current_lvt,new_hole->timestamp);
            print_event(new_hole);
            print_event(current_msg);
            printf("current_lp=%u,new_hole->receiver_id=%u\n",current_lp,new_hole->receiver_id);
            printf("current_msg->type=%u,new_hole->type=%u\n",current_msg->type,new_hole->type);
            gdb_abort;
        }
#endif

        if(current_msg->max_outgoing_ts < new_hole->timestamp)
            current_msg->max_outgoing_ts = new_hole->timestamp;

        msg_t*old_priority_message = flag_as_posted(new_hole,&flagged);

        #if HANDLE_INTERRUPT_WITH_CHECK==1
        enter_in_unpreemptable_zone();
        #endif

        _thr_pool.messages[i].father = NULL;

        #if REPORT == 1
        clock_timer_start(queue_op);
        #endif

        nbc_enqueue(nbcalqueue, new_hole->timestamp,new_hole, new_hole->receiver_id);
        
        #if REPORT == 1
        statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
        statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
        #endif

        #if HANDLE_INTERRUPT_WITH_CHECK==1
        exit_from_unpreemptable_zone(default_handler,INVALID);
        #endif

        #if DEBUG==1//not present in original version
        check_tie_breaker_not_zero(new_hole->tie_breaker);
        #endif

        
        if (flagged){
            posted=post_info_with_oldval(new_hole,old_priority_message);
        }
        #if IPI_SUPPORT==1
        if(posted)
            send_ipi_to_lp(new_hole);
        #else
        (void)posted;
        #endif
    }
    _thr_pool._thr_pool_count=0;//if we are not been interrupted we have implicit called queue_clean
}

#else//POSTING
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    //simtime_t max = current_msg->timestamp; //potrebbe essere fatto direttamente su current_msg->max_outgoing_ts
    
#if REPORT == 1
        clock_timer queue_op;
#endif
#if DEBUG==1//not present in original version
        unsigned int father_lp_idx=n_prc_tot;//invalid lp_idx
        if(LPS[current_lp]->state != LP_STATE_READY){
            printf("LP state is not ready in queue_deliver_msgs\n");
            gdb_abort;
        }
        if(current_msg==NULL){
            printf("current_msg NULL queuedeliver_msgs\n");
            gdb_abort;
        }
#endif
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {

        //new_hole = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
        //list_node_clean_by_content(new_hole); //NON DOVREBBE SERVIRE

        new_hole = _thr_pool.messages[i].father;
        _thr_pool.messages[i].father = NULL; 

        if(new_hole == NULL){
            printf("Out of memory in %s:%d", __FILE__, __LINE__);
            abort();        
        }
        #if DEBUG==1//not present in original version
            if(father_lp_idx==n_prc_tot){//first time set father_lp_idx with child_lp_idx
                father_lp_idx=new_hole->sender_id;
            }
            if(father_lp_idx!=new_hole->sender_id){//check if father_lp_idx is the same for each child
                printf("queue_deliver_msgs is flushing events with different father->lp_idx\n");
                gdb_abort;
            }
        #endif
        //memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        #if CONSTANT_CHILD_INVALIDATION==1
        new_hole->fatherEpoch = get_epoch_of_LP(current_lp);
        #else
        new_hole->fatherEpoch = LPS[current_lp]->epoch;
        #endif

        new_hole->monitor = (void *)0x0;
        new_hole->state = 0;
        new_hole->epoch = 0;
        new_hole->frame = 0;
        new_hole->tie_breaker = 0;
        new_hole->max_outgoing_ts = new_hole->timestamp;

#if DEBUG==1
        if(new_hole->timestamp < current_lvt){
            printf("Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f",current_lvt,new_hole->timestamp);
            print_event(new_hole);
            print_event(current_msg);
            printf("current_msg->timestamp=%lf,new_hole->timestamp=%lf\n",current_msg->timestamp,new_hole->timestamp);
            printf("current_lp=%u,new_hole->receiver_id=%u\n",current_lp,new_hole->receiver_id);
            printf("current_msg->type=%u,new_hole->type=%u\n",current_msg->type,new_hole->type);
            gdb_abort;
        }
#endif

        if(current_msg->max_outgoing_ts < new_hole->timestamp)
            current_msg->max_outgoing_ts = new_hole->timestamp;

#if REPORT == 1
        clock_timer_start(queue_op);
#endif
        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);
        #if DEBUG==1//not present in original version
        if(new_hole->tie_breaker==0){
            printf("flushed event with tie_breaker 0\n");
        }
        #endif

#if REPORT == 1
        statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
        statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
#endif
    }

    _thr_pool._thr_pool_count = 0;
}
#endif

#if CONSTANT_CHILD_INVALIDATION==1
bool is_valid(msg_t * event){
    bool validity;
    validity=  
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

    if(!validity)
        return validity;

    if(event->father==NULL)
        return validity;

    unsigned int father_lp=event->father->receiver_id;
    atomic_epoch_and_ts father_epoch_and_ts=atomic_load_epoch_and_ts(&(LPS[father_lp]->atomic_epoch_and_ts));
    unsigned int father_LP_epoch=get_epoch(father_epoch_and_ts);
    unsigned int father_epoch=event->father->epoch;

    if(father_LP_epoch==father_epoch)//we don't know if father will be re-executed
        return validity;

    //father_LP_epoch is greater than father_epoch,maybe father will be re-executed(if (LP_rollback_ts < event->father->timestamp))
    simtime_t LP_rollback_ts=get_timestamp(father_epoch_and_ts);

    if(LP_rollback_ts < event->father->timestamp) //father will be re-executed
        return false;

    return validity;
}

#else
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
#endif
