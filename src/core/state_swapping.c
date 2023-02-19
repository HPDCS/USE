#include "core.h"
#include <clock_constant.h>
#include <bitmap.h>
#include <unistd.h>
#include <simtypes.h>
#include <signal.h>
#include "queue.h"
#include "state_swapping.h"
#include <dymelor.h>
#include <timer.h>
#include <clock_constant.h>

#if CSR_CONTEXT == 1
#include <trap_based_usercontext.h>
#endif

#define PERIOD_SIGNAL_S 2 /// constant for triggering sync committed state reconstruction
#define PAGE_SIZE (4096)
#define STACK_SIZE (PAGE_SIZE<<2)

void csr_routine(void);

state_swapping_struct *state_swap_ptr;
__thread simtime_t commit_horizon_to_save;
__thread state_swapping_struct *private_swapping_struct;
__thread volatile unsigned int potential_locked_object; /// index of the lp locked in normal context
pthread_t ipi_tid; 


/**
 * Init subsystem for threads
 */

void init_thread_csr_state(void){
#if CSR_CONTEXT == 1
	void *alternate_stack;
	int res;
	alloc_memory_for_freezed_state();
	alternate_stack = malloc(STACK_SIZE);
	printf("TID:%d %d %p\n", tid, gettid(), private_swapping_struct);
    if (alternate_stack == NULL) {
	  printf("Cannot allcate memory for csr context\n"); 
	  exit(1);
  }
  memset(alternate_stack,0,STACK_SIZE);
  alternate_stack += STACK_SIZE-8; // stack increases with descreasing addresses
	
	res = sys_new_thread_context(csr_routine, alternate_stack);
	if(res == -2){
		printf("It seems that the syscall 'sys_new_thread_context' has not been installed\n");
		exit(1);
	}
	if(res == -1){
		printf("It seems that the syscall 'sys_new_thread_context' failed\n");
		exit(1);
	}
#endif
	private_swapping_struct = alloc_state_swapping_struct();
	private_swapping_struct->printed = 1;
	private_swapping_struct->worker_threads_out = n_cores;
}

void destroy_thread_csr_state(void){
#if CSR_CONTEXT == 1
	int res;
	printf("destroy kernel side user-context...");
	res = sys_destroy_thread_context();
	if(res == -2){
		printf("It seems that the syscall 'sys_destroy_thread_context' has not been installed\n");
		exit(1);
	}
	if(res == -1){
		printf("It seems that the syscall 'sys_destroy_thread_context' failed\n");
		exit(1);
	}
	printf("OK\n");
#endif
}

/**
 * This method handles the alarm that has been sent
 * it sets the state_swap_flag to 1 with a compare and swap
 * 
 */
void handle_signal(int sig) {

  #if CSR_CONTEXT == 0
	__sync_bool_compare_and_swap(&state_swap_ptr->state_swap_flag, 0, 1);
  #else 
  	if(!state_swap_ptr->csr_trigger_ts){
	    clock_timer_start(state_swap_ptr->csr_trigger_ts);
			printf("%p send_ipi_to_all SEND\n", state_swap_ptr);
			printf("TID:IPI cur:%p remaining %d entered %d\n", state_swap_ptr, state_swap_ptr->worker_threads_out, state_swap_ptr->worker_threads_in);

			sys_send_ipi(0);
		}
		else{
			printf("%p send_ipi_to_all MISSED\n", state_swap_ptr);
			printf("TID:IPI cur:%p remaining %d entered %d\n", state_swap_ptr, state_swap_ptr->worker_threads_out, state_swap_ptr->worker_threads_in);
		}
  #endif

    alarm(PERIOD_SIGNAL_S);    
}


/**
 * This method arms a signal and calls and alarm until the state_swap_flag has been set
 * 
 */
void signal_state_swapping() {


	//signal(SIGALRM, handle_signal);
	//alarm(PERIOD_SIGNAL_S);
	while(  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {
		usleep(PERIOD_SIGNAL_S*1000*1000);
	 	if(!state_swap_ptr->csr_trigger_ts){
	    clock_timer_start(state_swap_ptr->csr_trigger_ts);
			printf("%p send_ipi_to_all SEND\n", state_swap_ptr);
			printf("TID:IPI cur:%p remaining %d entered %d\n", state_swap_ptr, state_swap_ptr->worker_threads_out, state_swap_ptr->worker_threads_in);
          #if CSR_CONTEXT == 1
			sys_send_ipi(0);
          #else
            __sync_bool_compare_and_swap(&state_swap_ptr->state_swap_flag, 0, 1);
		  #endif
        }
		else{
			printf("%p send_ipi_to_all MISSED\n", state_swap_ptr);
			printf("TID:IPI cur:%p remaining %d entered %d\n", state_swap_ptr, state_swap_ptr->worker_threads_out, state_swap_ptr->worker_threads_in);
		}
	}

}

void init_state_swapping_struct(state_swapping_struct *sw_struct){
	sw_struct->reference_gvt = 0.0;
	for(int i=0; i<n_prc_tot;i++) reset_bit(sw_struct->lp_bitmap, i);

	sw_struct->counter_lp = n_prc_tot-1;
	sw_struct->state_swap_flag = 0;

	sw_struct->printed = 0;

	sw_struct->worker_threads_in = 0;
	sw_struct->worker_threads_out = 0;

	sw_struct->first_enter_ts = 0;
	sw_struct->last_enter_ts  = 0;
	sw_struct->first_exit_ts  = 0;
	sw_struct->last_exit_ts   = 0;

	sw_struct->csr_trigger_ts = 0;
}


void print_state_swapping_struct_metrics(void){
	if(!private_swapping_struct->printed && private_swapping_struct->worker_threads_out == n_cores)
	// && __sync_bool_compare_and_swap(&private_swapping_struct->printed, 0, 1))
	{
		private_swapping_struct->printed = 1;
		printf("%p CSR: trigger:%llu first_enter %llu last_enter:%llu first_exit:%llu last_exit_ts:%llu\n",
	
	private_swapping_struct,
	 private_swapping_struct->csr_trigger_ts ,
	(private_swapping_struct->first_enter_ts - private_swapping_struct->csr_trigger_ts)/CLOCKS_PER_US,
	(private_swapping_struct->last_enter_ts  - private_swapping_struct->csr_trigger_ts)/CLOCKS_PER_US,
	(private_swapping_struct->first_exit_ts  - private_swapping_struct->csr_trigger_ts)/CLOCKS_PER_US,
	(private_swapping_struct->last_exit_ts   - private_swapping_struct->csr_trigger_ts)/CLOCKS_PER_US	);
	}
}

/**
 * This method allocates a new structure for the state swapping routine
 * and fills the various fields
 * 
 * @return A state_swapping_struct pointer
 */
state_swapping_struct *alloc_state_swapping_struct() {

	state_swapping_struct *sw_struct = (state_swapping_struct *) malloc(sizeof(state_swapping_struct));

	sw_struct->lp_bitmap = allocate_bitmap(n_prc_tot);
	init_state_swapping_struct(sw_struct);
	sw_struct->counter_lp = n_prc_tot-1;

	return sw_struct;
}


/**
 * This method resets the fields of the state swapping struct
 * 
 * @param A state_swapping_struct pointer
 */
void reset_state_swapping_struct(state_swapping_struct *old_state_swap_ptr) {

    if(old_state_swap_ptr->worker_threads_in != n_cores) return;
	if(private_swapping_struct->worker_threads_out == n_cores){
        init_state_swapping_struct(private_swapping_struct);
		if (__sync_bool_compare_and_swap(&state_swap_ptr, old_state_swap_ptr, private_swapping_struct)){
			printf("I WON THE CHALLENGE -- TID:%d old: %p new: %p remaining %d entered %d\n", tid, old_state_swap_ptr, private_swapping_struct, old_state_swap_ptr->worker_threads_out, old_state_swap_ptr->worker_threads_in);
		  private_swapping_struct = old_state_swap_ptr;
		}
		else{
            private_swapping_struct->worker_threads_out == n_cores;
			printf("TID:%d I LOST EXCHANGE cur %p mine %p \n", tid, old_state_swap_ptr, private_swapping_struct );
		}
	}
	else
		printf("TID:%d I CANNOT EXCHANGE cur %p mine %p \n", tid, old_state_swap_ptr, private_swapping_struct );
}


/**
 * This method prints some fields of the state swapping structure, for debugging purposes
 * 
 * @param A state_swapping_struct pointer
 */
void print_state_swapping_struct(state_swapping_struct *ptr) {
	printf("state_swapping_struct->counter_lp %u --- state_swapping_struct->worker_threads_in %d --- ptr->worker_threads_out %d --- state_swapping_struct->state_swap_flag %u ---- state_swapping_struct->reference_gvt %f \n", ptr->counter_lp, ptr->worker_threads_in, ptr->worker_threads_out, ptr->state_swap_flag, ptr->reference_gvt);
}


/**
 * This method computes the reference gvt with a compare-and-swap operation
 * 
 * @param The current state_swapping_struct pointer
 */
void compute_reference_gvt(state_swapping_struct *cur_state_swap_ptr) {

	if (cur_state_swap_ptr->reference_gvt == 0) 
		__sync_bool_compare_and_swap(
							UNION_CAST(&(cur_state_swap_ptr->reference_gvt), unsigned long long *),
							UNION_CAST(0,unsigned long long),
							UNION_CAST(gvt, unsigned long long)
				);

}


/**
 * This method implements the logic of the output collection
 * for a given lp. It tries to lock the lp, and if it has not been already processed
 * it swaps the lp state, does the output collection and restores the state
 * 
 * @param The current state_swapping_struct pointer
 * @param lp The lp index to process
 * @param to_release True if the lock must be released afterwards, false otherwise
 */
void output_collection(state_swapping_struct *cur_state_swap_ptr, int cur_lp, bool to_release) {
	unsigned int old_current_lp = current_lp;

	if (!get_bit(cur_state_swap_ptr->lp_bitmap, cur_lp)) {
  	atomic_set_bit(cur_state_swap_ptr->lp_bitmap, cur_lp);

	  #if VERBOSE == 1
		printf("examining lp %d\n", cur_lp);
	  #endif 

		if(LPS[cur_lp]->commit_horizon_ts>0){
	
			simtime_t	commit_horizon_to_save = LPS[cur_lp]->commit_horizon_ts; ///save current commit horizon
			LPS[cur_lp]->commit_horizon_ts = cur_state_swap_ptr->reference_gvt; /// commit horizon to be used in check_OnGVT

			check_OnGVT(cur_lp);
			
			LPS[cur_lp]->commit_horizon_ts = commit_horizon_to_save; /// restore old commit horizon
			current_lp = old_current_lp;	
		}

	}

	if (to_release) unlock(cur_lp);

}

/**
 * This method implements the actual committed state reconstruction mechanism
 * every WT retrieves an LP to process and tries to do the output collection
 * every WT checks whether it has already locked some LP in normal context 
 * when the csr was triggered, if so it processes the output collection
 * otherwise it normally processes the LP retrieved
 * The routine stops when every LP has been correctly processed
 * and every WT restores its previous context
*/
void csr_routine(void){

	int cur_lp;
    clock_timer timer_val = 0;
	state_swapping_struct *cur_state_swap_ptr;

	while (1) {
      #if CSR_CONTEXT == 1
        raw_clock_timer_start(timer_val);
      #endif
		cur_state_swap_ptr = state_swap_ptr; 

		/// compute the reference gvt
		compute_reference_gvt(cur_state_swap_ptr);

	  #if VERBOSE == 1
		printf("gvt %f -- state_swap_ptr->reference_gvt %f\n", gvt, state_swap_ptr->reference_gvt);
	  #endif
        
        if(cur_state_swap_ptr->worker_threads_in == n_cores) goto exit;
		int enter_count = __sync_fetch_and_add(&cur_state_swap_ptr->worker_threads_in, 1);
		if(enter_count == 0        ) raw_clock_timer_start(state_swap_ptr->first_enter_ts);
		if(enter_count == n_cores-1) raw_clock_timer_start(state_swap_ptr->last_enter_ts);

		/// check if a lp was already locked in normal context
		if (haveLock(lp_lock[potential_locked_object])) { 
			printf("I have lock on %d\n", potential_locked_object);
			atomic_set_bit(cur_state_swap_ptr->lp_bitmap, potential_locked_object);
			log_freezed_state(potential_locked_object);
			output_collection(cur_state_swap_ptr, cur_lp, false);
			restore_freezed_state(potential_locked_object);
		} //TODO commented just for now


		cur_lp = __sync_fetch_and_add(&cur_state_swap_ptr->counter_lp, -1);

		while(cur_lp >= 0 
			/*&& 
			(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error */
			) 
		{ 

		  #if VERBOSE == 1
			printf("csr_routine lp %u\n", cur_lp);
		  #endif
			if (!get_bit(cur_state_swap_ptr->lp_bitmap, cur_lp) && tryLock(cur_lp)) {

				output_collection(cur_state_swap_ptr, cur_lp, true);
			}

			cur_lp = __sync_fetch_and_add(&cur_state_swap_ptr->counter_lp, -1);

		} 

	  #if CSR_CONTEXT == 0
		/// scan of the lp bitmap to check if some lp has missed the output collection
		unsigned int lp;

		for (lp = 0; lp < cur_state_swap_ptr->lp_bitmap->actual_len; lp++) {
			
			/*if((
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error) break;*/
			if (!get_bit(cur_state_swap_ptr->lp_bitmap, lp)) {

			  #if VERBOSE == 1
				printf("missing some lp %d\n", lp);
			  #endif

				output_collection(cur_state_swap_ptr, lp, true);

			}
		}
	  #endif
exit:
        if(cur_state_swap_ptr->worker_threads_out == n_cores) goto swap;
		int exit_count = __sync_fetch_and_add(&cur_state_swap_ptr->worker_threads_out, 1);
		if(exit_count == 0        ) raw_clock_timer_start(cur_state_swap_ptr->first_exit_ts);
		if(exit_count == n_cores-1) raw_clock_timer_start(cur_state_swap_ptr->last_exit_ts);

swap:
		reset_state_swapping_struct(cur_state_swap_ptr);
	  #if VERBOSE == 1
		print_state_swapping_struct(state_swap_ptr);
	  #endif
        
      #if CSR_CONTEXT == 1
        raw_clock_timer_value(timer_val);
        raw_update_offset(timer_val);
      #endif
		
        
		if (!CSR_CONTEXT) break;
end:
		change_context();

	} //end while(1)

	
	
}