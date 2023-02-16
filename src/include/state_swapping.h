#ifndef __STATE_SWAPPING_H
#define __STATE_SWAPPING_H



typedef struct state_swapping_struct {

	bitmap *lp_bitmap; /// bitmap representing all the lps
	int counter_lp; /// counter of the lps examined
	simtime_t reference_gvt; /// reference gvt for the output collection
	unsigned int state_swap_flag; /// flag for determining sync csr
	int worker_threads_in; /// number of threads executing the csr function
	int worker_threads_out; /// number of threads executing the csr function

} state_swapping_struct;

extern state_swapping_struct *state_swap_ptr;

extern __thread volatile unsigned int potential_locked_object; /// it contains the index of the lp locked in normal context

extern __thread simtime_t commit_horizon_to_save; 

extern void signal_state_swapping();

extern state_swapping_struct *alloc_state_swapping_struct();

extern void init_thread_csr_state(void);

extern void destroy_thread_csr_state(void);


#endif
