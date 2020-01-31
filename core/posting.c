#if IPI_POSTING==1

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

#include "hpdcs_utils.h"
#include "core.h"
#include "prints.h"
#include <events.h>
#include <posting.h>

void reset_LP_info(msg_t*event,int lp_idx){
    //I don't know if event is reliable or unreliable
    msg_t *evt;
    evt=LPS[lp_idx]->priority_message;
    if(event!=NULL && evt==event)
        CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
        (unsigned long)event,(unsigned long)NULL);
}

#if REPORT==1
void update_statistics(int lp_idx){
    statistics_post_lp_data(lp_idx,STAT_INFOS_POSTED_USEFUL,1);
    return;
}
#endif

/*#if IPI_POSTING_SINGLE_INFO==1
void swap_nodes(msg_t**event1,msg_t**event2,int i,int k,int*id_current_node,int*id_reliable){
    //swap two nodes with their relative indexes
    msg_t*temp_event;
    temp_event=*event1;
    *event1=*event2;
    *event2=temp_event;
    if(i==*id_current_node){
        *id_current_node=k;
    }
    else if(k==*id_current_node){
        *id_current_node=i;
    }
    if(i==*id_reliable){
        *id_reliable=k;
    }
    else if(k==*id_reliable){
        *id_reliable=i;
    }
    return;
}
#endif//IPI_POSTING_SINGLE_INFO*/

bool first_has_greater_ts(msg_t*event1,msg_t*event2){
    if(event1==NULL){
        return true;
    }
    else if(event2==NULL){
        return false;
    }
    //node1!=NULL and node2!=NULL
    if( (event1->timestamp < event2->timestamp)
                    || ( (event1->timestamp == event2->timestamp) && (event1->tie_breaker<=event2->tie_breaker) ) ){
        return false;
    }          
    return true;
}

/*#if DEBUG==1
void check_LP_info(msg_t **array_events,int num_events,int lp_idx,int id_current_node,int id_reliable,int id_unreliable){
    #if IPI_POSTING_SINGLE_INFO==1
        (void)id_unreliable;
    #endif
    for(int i=0;i<num_events;i++)
    {
        if(array_events[i]!=NULL)
        {
            #if IPI_POSTING_SINGLE_INFO==1
            if(array_events[i]->receiver_id!=lp_idx 
                //|| (( (array_events[i]->state & EXTRACTED) ==EXTRACTED) && (i==id_reliable))
                ||(array_events[i]->posted==UNPOSTED && (i==id_reliable) && (array_events[i]->state!=ELIMINATED) && (array_events[i]->state!=ANTI_MSG))
                || (array_events[i]->tie_breaker==0))
            #endif   
                {
                if(i==id_reliable){
                    printf("invalid LP id in array_events,reliable node,lp_idx=%d,LP id in event=%d\n",
                        lp_idx,array_events[i]->receiver_id);
                    print_event(array_events[i]);
                }
                else if(i==id_current_node){
                    printf("invalid LP id in array_events,current_node,lp_idx=%d,LP id in event=%d\n",
                        lp_idx,array_events[i]->receiver_id);
                    print_event(array_events[i]);
                }
                else{
                    printf("invalid id i=%d\n",i);
                }
                gdb_abort;
            }
        }
    }
}
#endif//DEBUG*/

void print_lp_id_in_thread_pool_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty thread pool\n");
    }
    for(unsigned int i=0;i<_thr_pool._thr_pool_count;i++){
        printf("LP id=%d\n",_thr_pool.messages[i].father->receiver_id);
    }
}

void post_information(msg_t*event){
    unsigned int lp_idx=event->receiver_id;
    while(1){
        simtime_t bound_ts;
        if(LPS[lp_idx]->bound!=NULL){
            bound_ts=LPS[lp_idx]->bound->timestamp;
        }
        msg_t*event_dest_LP=(msg_t *)LPS[lp_idx]->priority_message;
        if(event_dest_LP==NULL){
            if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                continue;
            //information modified
            #if REPORT==1
            statistics_post_lp_data(tid,STAT_INFOS_POSTED,1);
            #endif
            break;//
        }
        else if( event->timestamp<bound_ts && event->timestamp < event_dest_LP->timestamp)
        {//msg_dest_LP!=NULL
            if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                    (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                continue;
            //information modified
            #if REPORT==1
            statistics_post_lp_data(tid,STAT_INFOS_POSTED,1);
            #endif
            break;//break with posted=POSTED
        }
        break;//timestamp is greater,exit
    }
}

void post_information_with_straggler(msg_t*new_hole){
    int index_in_hash_table=new_hole->id_in_thread_pool_hash_table;
    msg_t*event_in_list=_thr_pool.collision_list[index_in_hash_table];
    if(event_in_list!=NULL && event_in_list==new_hole){
        //post information
        simtime_t bound_ts=0.0;
        unsigned int lp_id=event_in_list->receiver_id;
        
        /*while(1){
            //father event is safe?
            event_in_list->posted_valid=POSTED;
            if(LPS[lp_id]->bound!=NULL){
                bound_ts=LPS[lp_id]->bound->timestamp;
            }
            msg_t*event_dest_LP=(msg_t *)LPS[lp_id]->priority_message;
            if(event_dest_LP==NULL || event_dest_LP->posted_valid==UNPOSTED || event_dest_LP->receiver_id!=lp_id){
                if(CAS_x86((unsigned long long*)&(LPS[lp_id]->priority_message),
                    (unsigned long)event_dest_LP,(unsigned long)event_in_list)==false)//CAS failed
                    continue;
                //information modified
                #if REPORT==1
                statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
                #endif
                break;//break with posted=POSTED
            }
            else if( event_in_list->timestamp<bound_ts && event_in_list->timestamp < event_dest_LP->timestamp)
            {//msg_dest_LP!=NULL
                if(CAS_x86((unsigned long long*)&(LPS[lp_id]->priority_message),
                        (unsigned long)event_dest_LP,(unsigned long)event_in_list)==false)//CAS failed
                    continue;
                //information modified
                #if REPORT==1
                statistics_post_lp_data(current_lp,STAT_INFOS_POSTED,1);
                #endif
                break;//break with posted=POSTED
            }
            event_in_list->posted_valid=UNPOSTED;
            break;//timestamp is greater,exit
        }*/
        _thr_pool.collision_list[index_in_hash_table]=NULL;
    }
}

void insert_msg_in_hash_table(msg_t*msg_ptr){//open addressing
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
}


msg_t* LP_info_is_good(int lp_idx){
    //return NULL if info is not usable,else return info usable
    //TODO this function does not reset/unpost information because is not needed,write another function to reset/unpost info where is convenient
    msg_t*LP_info;
    LP_info=LPS[lp_idx]->priority_message;
    if(LP_info==NULL)
        return LP_info;
    #if DEBUG==1
        if(LP_info->receiver_id!=lp_idx){
            printf("invalid lp id in LP_info\n");
            gdb_abort;
        }
        if(LP_info->monitor==(void*)0x5AFE){
            printf("invalid monitor value in LP_info\n");
            gdb_abort;
        }
        /*if((LP_info->posted_valid==UNPOSTED) && (LP_info->state!=ANTI_MSG)){
            printf("info is UNPOSTED and event_state is not ANTI_MSG\n");
            gdb_abort;
        }
        if( (LP_info->state==ANTI_MSG) && (LP_info->posted_valid==POSTED)){
            printf("info is NEW_EVT && UNPOSTED\n");
            gdb_abort;
        }*/
    #endif
    if(LP_info->state==ELIMINATED)
        return NULL;
    simtime_t bound_ts=0.0;
    if(LP_info->tie_breaker==0)
        return NULL;
    if(LPS[lp_idx]->bound!=NULL){
        bound_ts=LPS[lp_idx]->bound->timestamp;
    }
    if (LP_info->timestamp >= bound_ts){
        return NULL;
    }
    return LP_info;
}

msg_t* get_best_LP_info_good(int lp_idx){
    if(safe)//if current event is safe then there is not info good
        return NULL;
    return LP_info_is_good(lp_idx);
}
#endif//IPI_POSTING