#ifndef HANDLE_INTERRUPT_H
#define HANDLE_INTERRUPT_H

#if IPI_HANDLE_INTERRUPT==1
#include <events.h>

#define ROLLBACK_ONLY 0x4//state reserved for dummy_bound


void make_LP_state_invalid_and_long_jmp(msg_t*restore_bound);
void reset_info_and_change_bound(unsigned int lid,msg_t*event);
void change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker);
void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event);
#endif

#endif