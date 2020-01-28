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

void reset_all_LP_info(msg_t*event,int lp_idx){
    //I don't know if event is reliable or unreliable
    #if IPI_POSTING_SINGLE_INFO==1
    msg_t *evt;
    evt=LPS[lp_idx]->best_evt_reliable;
    if(event!=NULL && evt==event)
        CAS_x86((unsigned long long*)&(LPS[lp_idx]->best_evt_reliable),
        (unsigned long)event,(unsigned long)NULL);
    #else
    msg_t *evt;
    evt=LPS[lp_idx]->best_evt_reliable;
    if(event!=NULL && evt==event)
        CAS_x86((unsigned long long*)&(LPS[lp_idx]->best_evt_reliable),
        (unsigned long)event,(unsigned long)NULL);
    else{
        evt=LPS[lp_idx]->best_evt_unreliable;
        if(event!=NULL && evt==event)
            CAS_x86((unsigned long long*)&(LPS[lp_idx]->best_evt_unreliable),
            (unsigned long)event,(unsigned long)NULL);
    }
    #endif//IPI_POSTING_SINGLE_INFO
}

void reset_LP_info_inside_lock(msg_t*event,bool reliable,int lp_idx){
    msg_t*evt;
    if(reliable){
        evt=LPS[lp_idx]->best_evt_reliable;
        if(event!=NULL && event==evt)
            CAS_x86((unsigned long long*)&(LPS[lp_idx]->best_evt_reliable),
            (unsigned long)event,(unsigned long)NULL);
    }
    else{
        evt=LPS[lp_idx]->best_evt_unreliable;
        if(event!=NULL && evt==event)
            CAS_x86((unsigned long long*)&(LPS[lp_idx]->best_evt_unreliable),
            (unsigned long)event,(unsigned long)NULL);
    }
    return;
}

#if REPORT==1
void update_statistics(bool reliable,int lp_idx){
    (void)reliable;
    statistics_post_lp_data(lp_idx,STAT_INFOS_POSTED_USEFUL,1);
    return;
}
#endif

#if IPI_POSTING_SINGLE_INFO==1
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
#else
void swap_nodes(msg_t**event1,msg_t**event2,int i,int k,int*id_current_node,int*id_reliable,int*id_unreliable){
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
    if(i==*id_unreliable){
        *id_unreliable=k;
    }
    else if(k==*id_unreliable){
        *id_unreliable=i;
    }
    return;
}
#endif//IPI_POSTING_SINGLE_INFO

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
#if IPI_POSTING_SINGLE_INFO==1
void sort_events(msg_t**array_events,int *num_elem,int*id_current_node,int*id_reliable,int lp_idx){
    //indexes must be already initialized
#if DEBUG==1
    if(array_events[*id_current_node]==NULL || array_events[*id_current_node]->tie_breaker==0){
        printf("curr_evt must be not NULL and with tie_breaker greater than 0\n");
        gdb_abort;
    }
#endif
    if((array_events[*id_reliable]!=NULL) && (array_events[*id_reliable]->tie_breaker==0)){
        array_events[*id_reliable]=NULL;
        (*num_elem)--;
        return;
    }
    //curr_evt !=NULL
    if(array_events[*id_current_node]== array_events[*id_reliable]){
        reset_LP_info_inside_lock(array_events[*id_reliable],true,lp_idx);
        array_events[*id_reliable]=NULL;
        (*num_elem)--;
        return;
    }
    if(first_has_greater_ts(array_events[*id_current_node],array_events[*id_reliable])){
                swap_nodes(&(array_events[*id_current_node]),&(array_events[*id_reliable]),*id_current_node,*id_reliable,id_current_node,id_reliable);   
    }
    return;
}
#else
void sort_events(msg_t**array_events,int *num_elem,int*id_current_node,int*id_reliable,int*id_unreliable,int lp_idx){
    //indexes must be already initialized
#if DEBUG==1
    if(array_events[*id_current_node]==NULL || array_events[*id_current_node]->tie_breaker==0){
        printf("curr_evt must be not NULL and with tie_breaker greater than 0\n");
        gdb_abort;
    }
    if((array_events[*id_reliable]==array_events[*id_unreliable]) && (array_events[*id_reliable]!=NULL)){
        printf("0x%lx,0x%lx\n",(long unsigned int)array_events[*id_reliable],(long unsigned int)array_events[*id_unreliable]);
        printf("reliable and not reliable must be different to each other\n");
        //può accadere se nodo inizialmente unreliable ,viene promosso a reliable e poi eseguito
        gdb_abort;
    }
#endif
    int initial_num_events=*num_elem;
    for(int i=1;i<initial_num_events;i++){
        if(array_events[i]!=NULL && (array_events[i]->tie_breaker==0)){
            array_events[i]=NULL;
            (*num_elem)--;
        }
    }
    //curr_evt !=NULL
    if(array_events[*id_current_node]== array_events[*id_reliable]){
        reset_LP_info_inside_lock(array_events[*id_reliable],true,lp_idx);
        array_events[*id_reliable]=NULL;
        #if IPI_POSTING_SINGLE_INFO!=1
        swap_nodes(&(array_events[*id_reliable]),&(array_events[*id_unreliable]),*id_reliable,*id_unreliable,id_current_node,id_reliable,id_unreliable);
        #endif
        (*num_elem)--;
    }
    if(array_events[*id_current_node]==array_events[*id_unreliable]){
        reset_LP_info_inside_lock(array_events[*id_unreliable],false,lp_idx);
        array_events[*id_unreliable]=NULL;
        (*num_elem)--;
    }

    //selection sort
    for(int i=0;i<*num_elem;i++){
        for(int k=i+1;k<*num_elem;k++){
            if(first_has_greater_ts(array_events[i],array_events[k])){
                swap_nodes(&(array_events[i]),&(array_events[k]),i,k,id_current_node,id_reliable,id_unreliable);   
            }
        }
    }
    return;
}
#endif//IPI_POSTING_SINGLE_INFO

#if DEBUG==1

void check_LP_info(msg_t **array_events,int num_events,int lp_idx,int id_current_node,int id_reliable,int id_unreliable){
    #if IPI_POSTING_SINGLE_INFO==1
        (void)id_unreliable;
    #endif
    for(int i=0;i<num_events;i++)
    {
        if(array_events[i]!=NULL)
        {
            #if IPI_POSTING_SINGLE_INFO!=1
            if(array_events[i]->receiver_id!=lp_idx 
                //|| (( (array_events[i]->state & EXTRACTED) ==EXTRACTED) && (i==id_reliable || i==id_unreliable)) //only state new_evt and extracted are allowed if node is not curent_node
                ||(array_events[i]->posted==UNPOSTED && (i==id_reliable || i==id_unreliable) && (array_events[i]->state!=ELIMINATED) && (array_events[i]->state!=ANTI_MSG))
                || (array_events[i]->tie_breaker==0))
            #else
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
                #if IPI_POSTING_SINGLE_INFO!=1
                else if(i==id_unreliable){
                    printf("invalid LP id in array_events,unreliable node,lp_idx=%d,LP id in event=%d\n",
                        lp_idx,array_events[i]->receiver_id);
                    print_event(array_events[i]);
                }
                #endif
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
#endif//DEBUG

#endif//IPI_POSTING