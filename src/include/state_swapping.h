#ifndef __STATE_SWAPPING_H
#define __STATE_SWAPPING_H

#include <core.h>
#include <clock_constant.h>
#include <bitmap.h>
#include <simtypes.h>
#include <signal.h>


#define PERIOD_SIGNAL_S 2


typedef struct state_swapping_struct {

	bitmap *lp_bitmap; /// bitmap representing all the lps
	unsigned int counter_lp; /// counter for the lps examined
	simtime_t reference_gvt; /// reference gvt for the output collection
	unsigned int state_swap_flag; /// flag for determining sync csr

} state_swapping_struct;

extern state_swapping_struct *state_swap_ptr;


/**
 * This method handles the alarm that has been sent
 * it sets the state_swap_flag to 1 with a compare and swap
 * 
 */
void handle_signal(int sig) {
	__sync_bool_compare_and_swap(&state_swap_ptr->state_swap_flag, 0, 1);
    alarm(PERIOD_SIGNAL_S);    
}


/**
 * This method arms a signal and calls and alarm until the state_swap_flag has been set
 * 
 */
static inline void signal_state_swapping() {

	signal(SIGALRM, handle_signal);
	alarm(PERIOD_SIGNAL_S);

	while(!state_swap_ptr->state_swap_flag);
}


/**
 * This method allocates a new structure for the state swapping routine
 * and fills the various fields
 * 
 * @return A state_swapping_struct instance
 */
static inline state_swapping_struct *alloc_state_swapping_struct() {

	state_swapping_struct *sw_struct = (state_swapping_struct *) malloc(sizeof(state_swapping_struct));

	sw_struct->lp_bitmap = allocate_bitmap(n_prc_tot);
	sw_struct->counter_lp = 0;
	sw_struct->reference_gvt = 0.0;
	sw_struct->state_swap_flag = 0;

	return sw_struct;
}




#endif
