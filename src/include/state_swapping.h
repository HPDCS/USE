#ifndef __STATE_SWAPPING_H
#define __STATE_SWAPPING_H

#include <timer.h>

typedef struct state_swapping_struct {

	int counter_lp; /// counter of the lps examined
	unsigned int state_swap_flag; /// flag for determining sync csr
	int pad;
	int printed;
	volatile unsigned int worker_threads_in; /// number of threads executing the csr function
	volatile unsigned int worker_threads_out; /// number of threads executing the csr function

	clock_timer first_enter_ts;
	clock_timer last_enter_ts;
	clock_timer first_exit_ts;
	clock_timer last_exit_ts;
	clock_timer avg_ts;
	
    clock_timer csr_trigger_ts;
	simtime_t reference_gvt; /// reference gvt for the output collection
	bitmap *lp_bitmap; /// bitmap representing all the lps

} state_swapping_struct;

extern pthread_t ipi_tid;

extern state_swapping_struct *state_swap_ptr;

extern __thread volatile unsigned int potential_locked_object; /// it contains the index of the lp locked in normal context

extern __thread simtime_t commit_horizon_to_save; 

extern void* signal_state_swapping(void *args);

extern state_swapping_struct *alloc_state_swapping_struct();

extern void init_thread_csr_state(void);

extern void destroy_thread_csr_state(void);

extern void print_state_swapping_struct_metrics(void);

extern void csr_routine(void);

#endif
