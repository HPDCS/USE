#if IPI_POSTING==1

#define remove_removed_if_current_node(curr_id,id_current_node,event,node) {\
                    if(curr_id==id_current_node){\
                        if(delete(nbcalqueue, node)){\
                            list_node_clean_by_content(event);\
                            list_insert_tail_by_content(to_remove_local_evts, event);\
                        }\
                    }; }

#define remove_if_current_node(curr_id,id_current_node,node) {\
                if(curr_id==id_current_node){\
                            delete(nbcalqueue, node);\
                        }; }

#if REPORT==1
#define update_LP_statistics(curr_id,id_reliable,id_unreliable,from_get_next_and_valid,lp_idx) {\
                    if(curr_id==id_reliable && from_get_next_and_valid==false){\
                        update_statistics(true,lp_idx);\
                    }\
                    else if(curr_id==id_unreliable && from_get_next_and_valid==false){\
                        update_statistics(false,lp_idx);\
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
void update_statistics(bool reliable,int lp_idx);
#endif

void reset_all_LP_info(msg_t*event,int lp_idx);
void reset_LP_info_inside_lock(msg_t*event,bool reliable,int lp_idx);
#if DEBUG==1
void check_LP_info(msg_t **array_events,int num_events,int lp_idx,int id_current_node,int id_reliable,int id_unreliable);
#endif
#if IPI_POSTING_SINGLE_INFO==1
void sort_events(msg_t**array_events,int *num_elem,int*id_current_node,int*id_reliable,int lp_idx);
#else
void sort_events(msg_t**array_events,int *num_elem,int*id_current_node,int*id_reliable,int*id_unreliable,int lp_idx);
#endif

#endif//IPI_POSTING