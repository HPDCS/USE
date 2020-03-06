#ifndef HANDLE_INTERRUPT_H
#define HANDLE_INTERRUPT_H

#include <jmp.h>
#include <queue.h>

//states reserved for field LP_simulation_state
#define INVALID 0x0
#define VALID 0x1
#define RESUMABLE 0x2

#define ROLLBACK_ONLY 0x4//state reserved for dummy_bound

void end_exposition_of_current_event(msg_t*event);
void start_exposition_of_current_event(msg_t*event);
msg_t* allocate_dummy_bound(unsigned int lp_idx);
void reset_info_and_change_bound(unsigned int lid,msg_t*event);
void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event);

#if DEBUG==1
bool dummy_bound_is_corrupted(int lp_idx);
bool bound_is_corrupted(int lp_idx);
#endif

void change_LP_state(msg_t*restore_bound,unsigned int new_state);
void reset_info_and_change_bound(unsigned int lid,msg_t*event);
void change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker);
void change_LP_state_and_long_jmp(msg_t*restore_bound,unsigned int new_state);
void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event);
void change_bound_with_current_msg();
#endif