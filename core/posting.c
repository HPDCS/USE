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

#if IPI_SUPPORT==1
#include <ipi.h>
#endif

/*void post_information_with_straggler(msg_t*new_hole){
    int index_in_hash_table=new_hole->id_in_thread_pool_hash_table;
    msg_t*event_in_list=_thr_pool.collision_list[index_in_hash_table];
    if(event_in_list!=NULL && event_in_list==new_hole){
        //post information
        simtime_t bound_ts=0.0;
        unsigned int lp_id=event_in_list->receiver_id;
        
        while(1){
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
        }
        _thr_pool.collision_list[index_in_hash_table]=NULL;
    }
}*/

/*void insert_msg_in_hash_table(msg_t*msg_ptr){//open addressing
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
}*/

/*void reset_LP_info(msg_t*event,int lp_idx){
    //I don't know if event is reliable or unreliable
    msg_t *evt;
    evt=LPS[lp_idx]->priority_message;
    if(event!=NULL && evt==event)
        CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
        (unsigned long)event,(unsigned long)NULL);
}*/

#if REPORT==1
void update_statistics(int lp_idx){
    statistics_post_lp_data(lp_idx,STAT_INFOS_POSTED_USEFUL,1);
    return;
}
#endif

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

#if DEBUG==1
void check_LP_info(int lp_idx,msg_t*priority_info){
    if( (priority_info!=NULL)){
        if( (priority_info->posted==NEVER_POSTED) || (priority_info->receiver_id!=lp_idx)){
            printf("invalid priority_info\n");
            print_event(priority_info);
            gdb_abort;
        }
    } 
        
}
#endif//DEBUG

void print_lp_id_in_thread_pool_list(){
    if(_thr_pool._thr_pool_count==0){
        printf("empty thread pool\n");
    }
    for(unsigned int i=0;i<_thr_pool._thr_pool_count;i++){
        printf("LP id=%d\n",_thr_pool.messages[i].father->receiver_id);
    }
}

void reset_priority_message(unsigned int lp_idx,msg_t*old_priority_message_to_reset){
    msg_t*new_priority_message=LPS[lp_idx]->priority_message;
    if(new_priority_message==old_priority_message_to_reset)
        CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
            (unsigned long)old_priority_message_to_reset,(unsigned long)NULL);
}

bool post_info_event_valid(msg_t*event){
    int value_posted=event->posted;
    if(value_posted==NEVER_POSTED && CAS_x86((unsigned long long*)&(event->posted),
        (unsigned long)NEVER_POSTED,(unsigned long)POSTED_VALID)==true){
        #if REPORT==1
        statistics_post_th_data(tid,STAT_INFOS_POSTED_ATTEMPT,1);
        #endif
        return post_information(event,true);
    }
    return false;
}

bool post_info_event_invalid(msg_t*event){
    int value_posted=event->posted;
    if(value_posted!=POSTED_INVALID && CAS_x86((unsigned long long*)&(event->posted),
        (unsigned long)value_posted,(unsigned long)POSTED_INVALID)==true){
        #if REPORT==1
        statistics_post_th_data(tid,STAT_INFOS_POSTED_ATTEMPT,1);
        #endif
        return post_information(event,true);
    }
    return false;
}

msg_t* flag_as_posted(msg_t*event,bool* flagged){
    unsigned int lp_idx=event->receiver_id;
    simtime_t bound_ts;
    if(LPS[lp_idx]->bound!=NULL){
        bound_ts=LPS[lp_idx]->bound->timestamp;
    }
    msg_t*event_dest_LP=(msg_t *)LPS[lp_idx]->priority_message;
    if(event_dest_LP==NULL){
        event->posted=POSTED_VALID;
        *flagged=true;
        #if REPORT==1
        statistics_post_th_data(tid,STAT_INFOS_POSTED_ATTEMPT,1);
        #endif
        return NULL;
    }
    else if( event->timestamp<bound_ts && event->timestamp < event_dest_LP->timestamp)
    {
        event->posted=POSTED_VALID;
        *flagged=true;
        #if REPORT==1
        statistics_post_th_data(tid,STAT_INFOS_POSTED_ATTEMPT,1);
        #endif
        return event_dest_LP;
    }
    *flagged=false;
    return NULL;
}

bool post_info_with_oldval(msg_t*event,msg_t*old_priority_message){
    unsigned int lp_idx=event->receiver_id;
    if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                (unsigned long)old_priority_message,(unsigned long)event)==false)//CAS failed
        return post_information(event,true);
    return false;
}

bool post_information(msg_t*event,bool retry_loop){
    unsigned int lp_idx=event->receiver_id;
    simtime_t bound_ts;
    msg_t*event_dest_LP;
    if(retry_loop){
        while(1){
            if(LPS[lp_idx]->bound!=NULL){
                bound_ts=LPS[lp_idx]->bound->timestamp;
            }
            event_dest_LP=(msg_t *)LPS[lp_idx]->priority_message;
            if(event_dest_LP==NULL){
                if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                    (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                    continue;
                //information modified
                #if REPORT==1
                statistics_post_th_data(tid,STAT_INFOS_POSTED,1);
                #endif
                return true;//
            }
            else if( event->timestamp<bound_ts && event->timestamp < event_dest_LP->timestamp)
            {//msg_dest_LP!=NULL
                if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                    (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                    continue;
                //information modified
                #if REPORT==1
                statistics_post_th_data(tid,STAT_INFOS_POSTED,1);
                #endif
                return true;
            }
            break;//timestamp is greater,exit
        }
        return false;
    }
    else{//one shot
        if(LPS[lp_idx]->bound!=NULL){
                bound_ts=LPS[lp_idx]->bound->timestamp;
        }
        event_dest_LP=(msg_t *)LPS[lp_idx]->priority_message;
        if(event_dest_LP==NULL){
            if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                return false;
            //information modified
            #if REPORT==1
            statistics_post_th_data(tid,STAT_INFOS_POSTED,1);
            #endif
            return true;//
        }
        else if( event->timestamp<bound_ts && event->timestamp < event_dest_LP->timestamp)
        {//msg_dest_LP!=NULL
            if(CAS_x86((unsigned long long*)&(LPS[lp_idx]->priority_message),
                    (unsigned long)event_dest_LP,(unsigned long)event)==false)//CAS failed
                return false;
            //information modified
            #if REPORT==1
            statistics_post_th_data(tid,STAT_INFOS_POSTED,1);
            #endif
            return true;
        }
        return false;//timestamp is greater,exit
    }
}

msg_t* LP_info_is_good(int lp_idx){
    //return NULL if info is not usable,else return info usable
    msg_t*LP_info;
    LP_info=LPS[lp_idx]->priority_message;
    if(LP_info==NULL)
        return LP_info;
    #if DEBUG==1
    if(LP_info->posted==NEVER_POSTED){
        printf("invalid LP_info,info is NEVER_POSTED\n");
        gdb_abort;
    }
    if(LP_info->receiver_id!=lp_idx){
        printf("invalid lp id in LP_info\n");
        gdb_abort;
    }
    if(LP_info->tie_breaker==0){
        printf("invalid tie_breaker in LP_info\n");
        gdb_abort;
    }
    #endif
    unsigned int info_state = LP_info->state;
    if( (info_state==ANTI_MSG && (LP_info->monitor==(void*)0xBA4A4A)) || info_state!=NEW_EVT)
        return NULL;
    simtime_t bound_ts=0.0;
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