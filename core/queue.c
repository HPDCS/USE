#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>

#include "queue.h"
#include "core.h"
#include "lookahead.h"
#include "hpdcs_utils.h"

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

#if IPI==1

void print_lp_id(void*data){
    msg_t*msg=(msg_t*)data;
    printf("LP id=%d\n",msg->receiver_id);
}

void print_lp_id_in_collision_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty collision list\n");
    }
    for(unsigned int i=0;i<THR_HASH_TABLE_SIZE;i++){
        if(_thr_pool.collision_list[i]==NULL)
            continue;
        print_list(_thr_pool.collision_list[i],print_lp_id);
    }
}

void print_lp_id_in_thread_pool_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty thread pool\n");
    }
    for(unsigned int i=0;i<_thr_pool._thr_pool_count;i++){
        printf("LP id=%d\n",_thr_pool.messages[i].father->receiver_id);
    }
}

#endif

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


#if IPI==1
    //event state set to NEW_EVT in queue_deliver_msgs

    //insert msg with minimum timestamp in collision list(if not exist alloc node,if exist swap if it has smaller timestamp
    int index=msg_ptr->receiver_id%THR_HASH_TABLE_SIZE;//return index of hash table
    struct node**head=&(_thr_pool.collision_list[index]);//head of collision list
    struct node*p=*head;
    msg_t*msg;

    while(p!=NULL){
        msg=(msg_t*)p->data;
        if(msg_ptr->receiver_id == msg->receiver_id){
            if( (msg_ptr->timestamp < msg->timestamp )
            // || ( (msg_ptr->timestamp == msg->timestamp ) && (msg_ptr->tie_breaker <= msg->tie_breaker) ) 
            //tie_breaker not exist yet:if 2 events (for same LP) have same minimum timestamp,keep pointer to first
            //it is correct because "child" events are generated sequentially in respect of father's execution
            //and these events are flushed to calqueue in order,so first event will has minimum tie_breaker
            //in respect of other events same timestamp same LP(generated from same father event)
            ){
                p->data=msg_ptr;
                break;
            }
            //same LP but greater timestamp
            break;
        }
        p=p->next;
    }
    if(p==NULL){
        //LP not found,add element to collision list
        struct node*new_node=get_new_node(msg_ptr);
        insert_at_head(new_node,head);
    }
    
#endif

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
    //simtime_t max = current_msg->timestamp; //potrebbe essere fatto direttamente su current_msg->max_outgoing_ts
    
//cristian:first flush event to calqueue,then post informations per LP
//flush all events generated to calqueue
#if REPORT == 1
		clock_timer queue_op;
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
        new_hole->fatherEpoch = LPS[current_lp]->epoch;

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

#if REPORT == 1
		statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
		statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
#endif
    }

    _thr_pool._thr_pool_count = 0;

//post per LP information if needed
//when event is flushed to calqueue it has also tie_breaker info
#if IPI == 1
    for(i = 0; i < THR_HASH_TABLE_SIZE; i++) {
        struct node**head=&(_thr_pool.collision_list[i]);
        struct node*p=*head;
        msg_t *msg,*msg_dest_LP;
        while(p!=NULL){
            msg=(msg_t*)p->data;
            unsigned short lp_id=msg->receiver_id;

            while(1){
                //father event is safe?
                if(safe){//safe info is overwrited only in fetch_internal() function
                    msg_dest_LP=LPS[lp_id]->best_evt_reliable;
                    if(msg_dest_LP==NULL){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                            (unsigned long)msg_dest_LP,(unsigned long)msg)==0)//CAS failed
                            continue;
                        //information modified
                        atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
                    }
                    else if( (msg->timestamp < msg_dest_LP->timestamp)
                        //check also tie_breaker,here tie_breaker is valid information
                    || ( (msg->timestamp == msg_dest_LP->timestamp) && (msg->tie_breaker<=msg_dest_LP->tie_breaker) ) ){//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                                (unsigned long)msg_dest_LP,(unsigned long)msg)==0)//CAS failed
                            continue;
                        //information modified
                        atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
                    }
                    break;//timestamp is greater,exit
                }
                else{//not safe
                    msg_dest_LP=LPS[lp_id]->best_evt_unreliable;
                    if(msg_dest_LP==NULL){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                            (unsigned long)msg_dest_LP,(unsigned long)msg)==0)//CAS failed
                            continue;
                        //modified
                        atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
                    }
                    else if( (msg->timestamp < msg_dest_LP->timestamp)
                    //check also tie_breaker,here tie_breaker is valid information
                    || ( (msg->timestamp == msg_dest_LP->timestamp) && (msg->tie_breaker<=msg_dest_LP->tie_breaker) ) ){//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                                (unsigned long)msg_dest_LP,(unsigned long)msg)==0)//CAS failed
                            continue;
                        //modified
                        atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
                    }
                    break;//timestamp is greater,exit
                }
            }
            p=p->next;
        }

        //free collision_list info
        if(_thr_pool.collision_list[i]!=NULL){
            free_memory_list(_thr_pool.collision_list[i]);
            _thr_pool.collision_list[i]=NULL;
        }
    }

#endif
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

