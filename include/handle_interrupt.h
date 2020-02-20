#ifndef HANDLE_INTERRUPT_H
#define HANDLE_INTERRUPT_H

#include <jmp.h>
#include <queue.h>

#define ROLLBACK_ONLY 0x4//state reserved for dummy_bound

void end_exposition_of_current_event(msg_t*event);
void start_exposition_of_current_event(msg_t*event);
msg_t* allocate_dummy_bound(unsigned int lp_idx);
void reset_info_and_change_bound(unsigned int lid,msg_t*event);
void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event);
bool dummy_bound_is_corrupted(int lp_idx);
bool bound_is_corrupted(int lp_idx);
void make_LP_state_invalid(msg_t*restore_bound);
void reset_info_and_change_bound(unsigned int lid,msg_t*event);
void change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker);
void make_LP_state_invalid_and_long_jmp(msg_t*restore_bound);
void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event);
void change_bound_with_current_msg();
#endif