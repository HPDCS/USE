#ifndef __FETCH_H__
#define __FETCH_H__

#include <events.h>
#include <lp/lp.h>

extern msg_t* get_next_and_valid(LP_state *lp_ptr, msg_t* current);

#endif