#ifndef IPI_POSTING_H
#define IPI_POSTING_H

#include <events.h>

#define NEVER_POSTED 0x0ULL
#define POSTED_VALID 0x1ULL
#define POSTED_INVALID 0x2ULL

void print_lp_id_in_thread_pool_list();
bool first_has_greater_ts(msg_t*event1,msg_t*event2);
msg_t* LP_info_is_good(int lp_idx);
void reset_LP_info(msg_t*event,int lp_idx);
msg_t* get_best_LP_info_good(int lp_idx);

#if REPORT==1
#define update_LP_statistics(from_info_posted,from_get_next_and_valid,lp_idx) {\
                    if(from_info_posted==true && from_get_next_and_valid==false){\
                        update_statistics(lp_idx);\
                    }; }
#endif

#if DEBUG==1
void check_LP_info(int lp_idx,msg_t*priority_info);
#endif

#if REPORT==1
void update_statistics(int lp_idx);
#endif

#define swap_event_with_priority_message(event,priority_message) { from_info_posted = true;\
        current_node_is_valid = false;\
        event = priority_message;\
        ts = event->timestamp;\
        ts = event->timestamp;\
        tb = event->tie_breaker;\
        in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));\
        validity = is_valid(event);\
        curr_evt_state = event->state;\
        safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx); }

bool post_information(msg_t*event,bool retry_loop);
bool post_info_with_oldval(msg_t*event,msg_t*old_priority_message);
void reset_priority_message(unsigned int lp_idx,msg_t*old_priority_message_to_reset);
msg_t* get_best_local_LP_info_good();
bool post_info_event_valid(msg_t*event);
bool post_info_event_invalid(msg_t*event);
msg_t* flag_as_posted(msg_t*event,bool* flagged);
#endif