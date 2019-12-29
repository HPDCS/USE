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

#if IPI_POSTING==1
#if IPI_COLLISION_LIST==1

void print_lp_id(void*data){
    msg_t*msg=(msg_t*)data;
    printf("LP id=%d\n",msg->receiver_id);
}
void print_lp_id_in_collision_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty collision list\n");
    }
    for(unsigned int i=0;i<MAX_THR_HASH_TABLE_SIZE;i++){
        if(_thr_pool.collision_list[i]==NULL)
            continue;
        print_list(_thr_pool.collision_list[i],print_lp_id);
    }
}



#endif//IPI_COLLISION_LIST

void print_lp_id_in_thread_pool_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty thread pool\n");
    }
    for(unsigned int i=0;i<_thr_pool._thr_pool_count;i++){
        printf("LP id=%d\n",_thr_pool.messages[i].father->receiver_id);
    }
}
#endif//IPI_POSTING
#if IPI_SUPPORT_STATISTICS==1
extern unsigned long num_sended_ipi;
extern unsigned long num_received_ipi;
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
#if IPI_POSTING==1
#if IPI_COLLISION_LIST==1
        if(_thr_pool.collision_list[i]!=NULL){
            free_memory_list(_thr_pool.collision_list[i]);//release memory only when hashtable uses collision_list
            _thr_pool.collision_list[i]=NULL;
        }
#endif
#endif
    }
	_thr_pool._thr_pool_count = 0;
}

#if IPI_POSTING==1
unsigned int next_pow_of_2(unsigned int y){
    //return y if y is a power of 2,else return minimum power of 2 greater than y
    //y must be greater than 0
    unsigned long x=y;
#if DEBUG==1
        if(x==0){
            printf("invalid argument in next_pow_of_2\n");
            gdb_abort;
        }
#endif
    if((x & (x-1))==0)
        return x;//x is a power of 2

    x--;

    if(sizeof(unsigned int)==4){
        x |= x >>1;
        x |= x >>2;
        x |= x >>4;
        x |= x >>8;
        x |= x >>16;
    }
    else if(sizeof(unsigned int)==8){
        x |= x >>1;
        x |= x >>2;
        x |= x >>4;
        x |= x >>8;
        x |= x >>16;
        x |= x >>32;
    }
    else{
        printf("invalid sizeof unsigned int\n");
        gdb_abort;
    }

    x++;
    return x;
}

unsigned int get_hash_table_size(unsigned int x){
    if(x==0)
        return x;
    return next_pow_of_2(x);
}

#if IPI_COLLISION_LIST==1
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    unsigned int hash_table_size=get_hash_table_size(_thr_pool._thr_pool_count);
#if REPORT == 1
    clock_timer queue_op;
#endif
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {
        new_hole = _thr_pool.messages[i].father;

        if(new_hole == NULL){
            printf("Out of memory in %s:%d", __FILE__, __LINE__);
            abort();        
        }
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        new_hole->fatherEpoch = LPS[current_lp]->epoch;

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

#if REPORT == 1
        clock_timer_start(queue_op);
#endif
        //insert node with minimum timestamp in collision list(if not exist alloc node,if exist swap if it has smaller timestamp)
        int index=new_hole->receiver_id & (hash_table_size-1);//return index of hash table
        struct node**head=&(_thr_pool.collision_list[index]);//head of collision list
        struct node*p=*head;
        msg_t *event_in_list;//bucket in collision list

        while(p!=NULL){
            event_in_list=(msg_t *)p->data;//node inside collision list
            if(new_hole->receiver_id==event_in_list->receiver_id)//node for same LP
            {
                if( new_hole->timestamp < event_in_list->timestamp )
                {//update min node
                    p->data=new_hole;//update node information
                    break;
                }
                //same LP but greater timestamp
                break;
            }
            p=p->next;
        }
        if(p==NULL){
            //LP not found,add element to collision list
            struct node*new_node=get_new_node(new_hole);
            insert_at_head(new_node,head);
        }
        

#if REPORT == 1
        statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
        statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
#endif
    }
//post per LP information if needed
    for(i = 0; i < hash_table_size; i++) {
        struct node**head=&(_thr_pool.collision_list[i]);//get collision_list[i]
        struct node*p=*head;
        msg_t *event_in_list,*event_dest_LP;
        simtime_t bound_ts=INFTY;
        while(p!=NULL){
            event_in_list=(msg_t *)p->data;//node inside collision list
            unsigned int lp_id=event_in_list->receiver_id;
            if(LPS[lp_id]->bound!=NULL){
                bound_ts=LPS[lp_id]->bound->timestamp;
            }
            while(1){
                event_in_list->posted=POSTED;
                //father event is safe?
                if(safe){//safe info is overwrited only in fetch_internal() function
                    event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_reliable;
                    if(event_dest_LP==NULL || event_dest_LP->posted==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
#if IPI_POSTING_STATISTICS==1
                        atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
#endif
                        break;//break with posted=POSTED
                    }
                    else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                    {//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                                (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
#if IPI_POSTING_STATISTICS==1
                        atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
#endif
                        break;//break with posted=POSTED
                    }
                    event_in_list->posted=UNPOSTED;
                    break;//timestamp is greater,exit
                }
                else{//not safe
                    event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_unreliable;
                    if(event_dest_LP==NULL || event_dest_LP->posted==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
#if IPI_POSTING_STATISTICS==1
                        atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
#endif
                        break;//break with posted=POSTED
                    }
                    else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                    {//msg_dest_LP!=NULL
                        if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                                (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                            continue;
                        //information modified
#if IPI_POSTING_STATISTICS==1
                        atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
#endif
                        break;//break with posted=POSTED
                    }
                    event_in_list->posted=UNPOSTED;
                    break;//timestamp is greater,exit
                }
            }
            p=p->next;
        }

        //free collision_list info
        if(_thr_pool.collision_list[i]!=NULL){
            free_memory_list(_thr_pool.collision_list[i]);//release memory only when hashtable uses collision_list
            _thr_pool.collision_list[i]=NULL;
        }
    }
    //flush all messagges in calendarqueue
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {
        new_hole = _thr_pool.messages[i].father;
        _thr_pool.messages[i].father = NULL;
        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);
#if IPI_SUPPORT==1
        //n.b. con le interruzioni e collision list bisogna liberare la memoria di tutte le liste di collisione
        //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
        unsigned int lcl_tid=(lp_lock[(new_hole->receiver_id)*CACHE_LINE_SIZE/4]);
        if(lcl_tid>0){
            #if IPI_SUPPORT_STATISTICS==1
            atomic_inc_x86((atomic_t *)&num_sended_ipi);
            #endif
            if (syscall(174, lcl_tid-1))
                printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
        }
#endif
    }
    _thr_pool._thr_pool_count=0;
}
#else//mode OPEN_ADDRESSING
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    unsigned int hash_table_size=get_hash_table_size(_thr_pool._thr_pool_count);
#if REPORT == 1
		clock_timer queue_op;
#endif
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {

        new_hole = _thr_pool.messages[i].father;
        if(new_hole == NULL){
            printf("Out of memory in %s:%d", __FILE__, __LINE__);
            abort();        
        }
        new_hole->father = current_msg;
        new_hole->fatherFrame = LPS[current_lp]->num_executed_frames;
        new_hole->fatherEpoch = LPS[current_lp]->epoch;

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

#if REPORT == 1
        clock_timer_start(queue_op);
#endif

#if DEBUG==1
        if(MAX_THR_HASH_TABLE_SIZE<THR_POOL_SIZE){
            printf("THR_HASH_TABLE_SIZE must be equals or greater than THR_POOL_SIZE in OPEN_ADDRESSING\n");
            gdb_abort;
        }
#endif//DEBUG
        //insert node with minimum timestamp in collision list(if not exist alloc node,if exist swap if it has smaller timestamp)
        int start_index=new_hole->receiver_id & (hash_table_size-1);//return index of hash table
        for(unsigned int i=start_index;i<hash_table_size+start_index;i++){
            int new_index=i & (hash_table_size-1);
            msg_t*event_in_list=_thr_pool.collision_list[new_index];
            if(event_in_list==NULL){//empty position,not exist any event with same LP id
                _thr_pool.collision_list[new_index]=new_hole;
                break;
            }
            else if(event_in_list->receiver_id==new_hole->receiver_id){//events have same LP id
                if( new_hole->timestamp < event_in_list->timestamp ) 
                {//update min event
                    _thr_pool.collision_list[new_index]=new_hole;//update node information
                    break;
                }
                //same LP but greater timestamp
                break;
            }
        }       

#if REPORT == 1
        statistics_post_lp_data(current_lp, STAT_CLOCK_ENQUEUE, (double)clock_timer_value(queue_op));
        statistics_post_lp_data(current_lp, STAT_EVENT_ENQUEUE, 1);
#endif
    }
    //post per LP info
    for(i = 0; i < hash_table_size; i++) {
        msg_t *event_in_list,*event_dest_LP;
        event_in_list=_thr_pool.collision_list[i];
        simtime_t bound_ts=INFTY;
        if(event_in_list==NULL){
            continue;//empty element,go to next element
        }
        unsigned int lp_id=event_in_list->receiver_id;
        if(LPS[lp_id]->bound!=NULL){
                bound_ts=LPS[lp_id]->bound->timestamp;
            }
        while(1){
            //father event is safe?
            event_in_list->posted=POSTED;
            if(safe){//safe info is overwrited only in fetch_internal() function
                event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_reliable;
                if(event_dest_LP==NULL || event_dest_LP->posted==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                    if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                        (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                        continue;
                    //information modified
#if IPI_POSTING_STATISTICS==1
                    atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
#endif
                    break;//break with posted=POSTED
                }
                else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                {//msg_dest_LP!=NULL
                    if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_reliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                        continue;
                    //information modified
#if IPI_POSTING_STATISTICS==1
                    atomic_inc_x86 ((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_reliable));
#endif
                    break;//break with posted=POSTED
                }
                event_in_list->posted=UNPOSTED;
                break;//timestamp is greater,exit
            }
            else{//not safe
                event_dest_LP=(msg_t *)LPS[lp_id]->best_evt_unreliable;
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
#if IPI_POSTING_STATISTICS==1
                    atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
#endif
                    break;//break with posted=POSTED
                }
                else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
                {//msg_dest_LP!=NULL
                    if(CAS_x86((unsigned long long*)&(LPS[lp_id]->best_evt_unreliable),
                            (unsigned long)event_dest_LP,(unsigned long)event_in_list)==0)//CAS failed
                        continue;
                    //information modified
#if IPI_POSTING_STATISTICS==1
                    atomic_inc_x86((atomic_t *)&(LPS[lp_id]->num_times_modified_best_evt_unreliable));
#endif
                    break;//break with posted=POSTED
                }
                event_in_list->posted=UNPOSTED;
                break;//timestamp is greater,exit
            }
        }
        _thr_pool.collision_list[i]=NULL;
    }
    //flush all messagges in calendarqueue
    for(i = 0; i < _thr_pool._thr_pool_count; i++) {
        new_hole = _thr_pool.messages[i].father;
        _thr_pool.messages[i].father = NULL;
        nbc_enqueue(nbcalqueue, new_hole->timestamp, new_hole, new_hole->receiver_id);
#if IPI_SUPPORT==1
        //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
        unsigned int lcl_tid=(lp_lock[(new_hole->receiver_id)*CACHE_LINE_SIZE/4]);
        if(lcl_tid>0){
            #if IPI_SUPPORT_STATISTICS==1
            atomic_inc_x86((atomic_t *)&num_sended_ipi);
            #endif
            if (syscall(174, lcl_tid-1))
                printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
        }
#endif
    }
    _thr_pool._thr_pool_count=0;
}
#endif//IPI_COLLISION_LIST
#else//IPI_POSTING
void queue_deliver_msgs(void) {
    msg_t *new_hole;
    unsigned int i;
    //simtime_t max = current_msg->timestamp; //potrebbe essere fatto direttamente su current_msg->max_outgoing_ts
    
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
#if IPI_SUPPORT==1
        //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
        unsigned int lcl_tid=(lp_lock[(new_hole->receiver_id)*CACHE_LINE_SIZE/4]);
        if(lcl_tid>0){
            #if IPI_SUPPORT_STATISTICS==1
            atomic_inc_x86((atomic_t *)&num_sended_ipi);
            #endif
            if (syscall(174, lcl_tid-1))
                printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
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

