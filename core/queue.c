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

#if IPI_LONG_JMP==1
#include "jmp.h"
extern __thread cntx_buf cntx_loop;
#endif

#if IPI_PREEMPT_COUNTER==1
extern __thread unsigned long long * preempt_count_ptr;
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

#if POSTING==1
void print_lp_id_in_thread_pool_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty thread pool\n");
    }
    for(unsigned int i=0;i<_thr_pool._thr_pool_count;i++){
        printf("LP id=%d\n",_thr_pool.messages[i].father->receiver_id);
    }
}
#endif//IPI_POSTING

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
    list_node_clean_by_content(msg_ptr); //NON DOVREBBE SERVIRE    

	//TODO: slaballoc al posto della malloc per creare il descrittore dell'evento
	_thr_pool.messages[_thr_pool._thr_pool_count++].father = msg_ptr;

    msg_ptr->sender_id = current_lp;
    msg_ptr->receiver_id = receiver;
    msg_ptr->timestamp = timestamp;
    msg_ptr->data_size = event_size;
    msg_ptr->type = event_type;

    memcpy(msg_ptr->data, event_content, event_size);

    #if IPI_POSTING==1
    unsigned int hash_table_size=MAX_THR_HASH_TABLE_SIZE;
    #if DEBUG==1
    if((MAX_THR_HASH_TABLE_SIZE & (MAX_THR_HASH_TABLE_SIZE-1))!=0){
        printf("MAX_THR_HASH_TABLE_SIZE is not a power of 2\n");
        gdb_abort;
    }
    #endif
    int start_index=msg_ptr->receiver_id & (hash_table_size-1);//return index of hash table
    for(unsigned int i=start_index;i<hash_table_size+start_index;i++){
        int new_index=i & (hash_table_size-1);
        msg_t*event_in_list=_thr_pool.collision_list[new_index];
        if(event_in_list==NULL){//empty position,not exist any event with same LP id
            _thr_pool.collision_list[new_index]=msg_ptr;
            msg_ptr->id_in_thread_pool_hash_table=new_index;
            break;
        }
        else if(event_in_list->receiver_id==msg_ptr->receiver_id){//events have same LP id
            if( msg_ptr->timestamp < event_in_list->timestamp ) 
            {//update min event
                _thr_pool.collision_list[new_index]=msg_ptr;//update node information
                msg_ptr->id_in_thread_pool_hash_table=new_index;
                break;
            }
            //same LP but greater timestamp
            break;
        }
    }
    #endif//IPI_POSTING
}

void queue_clean(){ 
    unsigned int i = 0;
    msg_t* current;
    #if IPI_POSTING==1 || DEBUG==1
    unsigned int father_lp_idx=n_prc_tot;
    #endif
    for (; i<_thr_pool._thr_pool_count; i++)
    {
        current = _thr_pool.messages[i].father;
        if(current != NULL)
        {
            #if IPI_POSTING==1 || DEBUG==1
            if(father_lp_idx==n_prc_tot){
                father_lp_idx=current->sender_id;
            }
            #endif
            #if DEBUG==1
            if(father_lp_idx!=current->sender_id){
                printf("cleaning queue containing events with different lp_idx\n");
                gdb_abort;
            }
            #endif
            list_node_clean_by_content(current); 
            current->state = -1;
            current->data_size = tid+1;
            current->max_outgoing_ts = 0;
            list_insert_tail_by_content(freed_local_evts, current);
            _thr_pool.messages[i].father = NULL;
            #if IPI_POSTING==1
            int index_in_hash_table=current->id_in_thread_pool_hash_table;
            _thr_pool.collision_list[index_in_hash_table]=NULL;
            #if REPORT==1
            statistics_post_lp_data(father_lp_idx,STAT_EVENT_NOT_FLUSHED,1);
            #endif
            #endif

        }
    }
	_thr_pool._thr_pool_count = 0;
}
#if IPI_SUPPORT==1
void send_ipi_to_lp(int lp_idx,simtime_t ts_event){
    //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
    simtime_t ts_evt_exec_dest = LPS[lp_idx]->ts_current_msg_in_execution;
    unsigned int lck_tid=(lp_lock[(lp_idx)*CACHE_LINE_SIZE/4]);
    if(lck_tid>0 && ts_event<ts_evt_exec_dest){
        #if REPORT==1
        statistics_post_th_data(tid,STAT_IPI_SENDED,1);
        #endif
        if (syscall(174, lck_tid-1))
            printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
    }
}
#endif

#if IPI_POSTING==1
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
#if REPORT == 1
        clock_timer queue_op;
#endif
#if DEBUG==1//not present in original version
        if(LPS[current_lp]->state != LP_STATE_READY){
            printf("LP state is not ready in queue_deliver_msgs\n");
            gdb_abort;
        }
        #if IPI_HANDLE_INTERRUPT==1
        if(LPS[current_lp]->bound_pre_rollback!=NULL || current_msg==NULL){
                    printf("bound_pre_rollback is not NULL or current_msg NULL queuedeliver_msgs\n");
                    gdb_abort;
        }
        #endif
#endif
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {
        #if IPI_POSTING_SYNC_CHECK_FUTURE==1 && IPI_INTERRUPT_FUTURE==1
            if(*preempt_count_ptr==PREEMPT_COUNT_CODE_INTERRUPTIBLE){
                #if REPORT==1
                statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_IN_FUTURE,1);
                #endif
                #if VERBOSE>0
                printf("sync check future queue_deliver_msgs\n");
                #endif
                msg_t*evt=get_best_LP_info_good(current_lp);
                if(evt!=NULL){
                    LPS[current_lp]->LP_state_is_valid=false;
                    LPS[current_lp]->dummy_bound->state=NEW_EVT;
                    wrap_long_jmp(&cntx_loop,CFV_ALREADY_HANDLED);
                }
            }
        #endif //IPI_POSTING_SYNC_CHECK_FUTURE

        new_hole = _thr_pool.messages[i].father;
        if(new_hole == NULL){
            printf("Out of memory in %s:%d", __FILE__, __LINE__);
            abort();
        }
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        #if IPI_CONSTANT_CHILD_INVALIDATION==1
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
        new_hole->posted=UNPOSTED;
#if DEBUG==1
        if(new_hole->timestamp < current_lvt){ printf(RED("1Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
        if(new_hole->timestamp < current_lvt){ printf(RED("3Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
#endif

        if(current_msg->max_outgoing_ts < new_hole->timestamp)
            current_msg->max_outgoing_ts = new_hole->timestamp;

        int index_in_hash_table=new_hole->id_in_thread_pool_hash_table;
        msg_t*event_in_list=_thr_pool.collision_list[index_in_hash_table];
        if(event_in_list!=NULL && event_in_list==new_hole){
            //post information
            simtime_t bound_ts=0.0;
            unsigned int lp_id=event_in_list->receiver_id;
            
            while(1){
                //father event is safe?
                event_in_list->posted=POSTED;
                if(LPS[lp_id]->bound!=NULL){
                    bound_ts=LPS[lp_id]->bound->timestamp;
                }
                if(safe){//safe info is overwrited only in fetch_internal() function
                    msg_t*event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_reliable;
                    if(event_dest_LP==NULL || event_dest_LP->posted==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
                        #if REPORT==1
                        statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
                        #endif
                        break;//break with posted=POSTED
                    }
                    else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                    {//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                                (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
                        #if REPORT==1
                        statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
                        #endif
                        break;//break with posted=POSTED
                    }
                    event_in_list->posted=UNPOSTED;
                    break;//timestamp is greater,exit
                }
                else{//not safe
                    msg_t*event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_unreliable;
                    #if DEBUG==1
                        if(event_dest_LP!=NULL && event_dest_LP->receiver_id!=lp_id){
                            printf("invalid tag and LP id in flush thread_pool_hash_table unreliable node\n");
                            gdb_abort;
                        }
                    #endif
                    if(event_dest_LP==NULL || event_dest_LP->posted==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
    #if REPORT==1
                        statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
    #endif
                        break;//break with posted=POSTED
                    }
                    else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                    {//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                                (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
    #if REPORT==1
                        statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
    #endif
                        break;//break with posted=POSTED
                    }
                    event_in_list->posted=UNPOSTED;
                    break;//timestamp is greater,exit
                } 
            }
            _thr_pool.collision_list[index_in_hash_table]=NULL;
        }
        _thr_pool.messages[i].father = NULL;
        #if REPORT == 1
        clock_timer_start(queue_op);
        #endif

        nbc_enqueue(nbcalqueue, new_hole->timestamp,new_hole, new_hole->receiver_id);
        
        #if REPORT == 1
        statistics_post_lp_data(current_lp,STAT_EVENT_FLUSHED,1);
        statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
        statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
        #endif

        #if DEBUG==1//not present in original version
        if(new_hole->tie_breaker==0){
            printf("flushed event with tie_breaker 0\n");
        }
        #endif
        #if IPI_SUPPORT==1
            send_ipi_to_lp(new_hole->receiver_id,new_hole->timestamp);
        #endif
    }
    _thr_pool._thr_pool_count=0;
}
#else//IPI_POSTING
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    //simtime_t max = current_msg->timestamp; //potrebbe essere fatto direttamente su current_msg->max_outgoing_ts
    
#if REPORT == 1
        clock_timer queue_op;
#endif
#if DEBUG==1//not present in original version
        if(LPS[current_lp]->state != LP_STATE_READY){
            printf("LP state is not ready in queue_deliver_msgs\n");
            gdb_abort;
        }
        #if IPI_HANDLE_INTERRUPT==1
        if(LPS[current_lp]->bound_pre_rollback!=NULL || current_msg==NULL){
                    printf("bound_pre_rollback is not NULL or current_msg NULL queuedeliver_msgs\n");
                    gdb_abort;
        }
        #endif
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
        //memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        #if IPI_CONSTANT_CHILD_INVALIDATION==1
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
        if(new_hole->timestamp < current_lvt){ printf(RED("1Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
        //if(new_hole->timestamp != _thr_pool.messages[i].timestamp){ printf(RED("2Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
        if(new_hole->timestamp < current_lvt){ printf(RED("3Sto generando eventi nel passato!!! LVT:%f NEW_TS:%f"),current_lvt,new_hole->timestamp); gdb_abort;}
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
#if IPI_SUPPORT==1
        send_ipi_to_lp(new_hole->receiver_id,new_hole->timestamp);
#endif
    }

    _thr_pool._thr_pool_count = 0;
}
#endif

#if IPI_CONSTANT_CHILD_INVALIDATION==1
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
    if(father_LP_epoch==father_epoch)
        return validity;
    simtime_t LP_rollback_ts=get_timestamp(father_epoch_and_ts);
    if(LP_rollback_ts<event->father->timestamp) //father will be re-executed
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