#ifndef IPI_POSTING_H
#define IPI_POSTING_H

#include <events.h>

#define UNPOSTED false
#define POSTED true

#define NEVER_POSTED 0x0
#define POSTED_VALID 0x1
#define POSTED_INVALID 0x2

#define unpost_event(event) { \
    if(event!=NULL && event->posted==POSTED){\
        event->posted=UNPOSTED;\
        }; }

#define unpost_event_inside_lock(event) { \
        unpost_event(event);\
     }

#define remove_removed_if_current_node(from_info_posted,event,node) {\
                    if(!from_info_posted){\
                        if(delete(nbcalqueue, node)){\
                            list_node_clean_by_content(event);\
                            list_insert_tail_by_content(to_remove_local_evts, event);\
                        }\
                    }; }

#define remove_if_current_node(from_info_posted,node) {\
                if(!from_info_posted){\
                            delete(nbcalqueue, node);\
                        }; }

#if REPORT==1
#define update_LP_statistics(from_info_posted,from_get_next_and_valid,lp_idx) {\
                    if(from_info_posted==true && from_get_next_and_valid==false){\
                        update_statistics(lp_idx);\
                    }; }
#endif

#define goto_next_information(lp_idx,event,curr_id,id_reliable,id_unreliable) {\
                reset_and_unpost(lp_idx,event,curr_id,id_reliable,id_unreliable);\
                add_lp_unsafe_set(lp_idx);\
                curr_id++;\
                goto retry_with_new_node;\
            }

#define reset_and_unpost(lp_idx,event,curr_id,id_reliable,id_unreliable) {\
                reset_current_LP_info_inside_lock(curr_id,id_reliable,id_unreliable,event,lp_idx);\
                unpost_event_inside_lock(event);\
            }

#define reset(lp_idx,event,curr_id,id_reliable,id_unreliable) {\
                reset_current_LP_info_inside_lock(curr_id,id_reliable,id_unreliable,event,lp_idx);\
            }

#define reset_current_LP_info_inside_lock(curr_id,id_reliable,id_unreliable,event,lp_idx) {\
                        if(curr_id==id_reliable){\
                            reset_LP_info_inside_lock(event,true,lp_idx);\
                        }\
                        else if(curr_id==id_unreliable){\
                            reset_LP_info_inside_lock(event,false,lp_idx);\
                        }; }

#define reset_all_LP_info_inside_lock(event,lp_idx) {\
                reset_all_LP_info(event,lp_idx);\
            }

#if REPORT==1
void update_statistics(int lp_idx);
#endif

void reset_all_LP_info(msg_t*event,int lp_idx);
#if DEBUG==1
void check_LP_info(msg_t **array_events,int num_events,int lp_idx,int id_current_node,int id_reliable,int id_unreliable);
#endif

void print_lp_id_in_thread_pool_list();
void post_information_with_straggler(msg_t*new_hole);
void insert_msg_in_hash_table(msg_t*msg_ptr);
bool first_has_greater_ts(msg_t*event1,msg_t*event2);
msg_t* LP_info_is_good(int lp_idx);
void reset_LP_info(msg_t*event,int lp_idx);
msg_t* get_best_LP_info_good(int lp_idx);
void post_information(msg_t*event);
msg_t* get_best_local_LP_info_good();

#endif