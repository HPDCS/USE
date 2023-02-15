#include "core.h"
#include <clock_constant.h>
#include <bitmap.h>
#include <unistd.h>
#include <simtypes.h>
#include <signal.h>
#include "queue.h"
#include "state_swapping.h"



#define PERIOD_SIGNAL_S 2 /// constant for triggering sync committed state reconstruction

state_swapping_struct *state_swap_ptr;

__thread simtime_t commit_horizon_to_save;


/**
 * This method handles the alarm that has been sent
 * it sets the state_swap_flag to 1 with a compare and swap
 * 
 */
void handle_signal(int sig) {

  #if CSR_CONTEXT == 0
	__sync_bool_compare_and_swap(&state_swap_ptr->state_swap_flag, 0, 1);
  #else 
	//send_IPI_to_all();
	printf("send_ipi_to_all\n");
  #endif

    alarm(PERIOD_SIGNAL_S);    
}


/**
 * This method arms a signal and calls and alarm until the state_swap_flag has been set
 * 
 */
void signal_state_swapping() {


	signal(SIGALRM, handle_signal);
	alarm(PERIOD_SIGNAL_S);
	while(  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {
		usleep(PERIOD_SIGNAL_S*500);
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
	sw_struct->counter_lp = n_prc_tot-1;
	sw_struct->reference_gvt = 0.0;
	sw_struct->state_swap_flag = 0;
	sw_struct->worker_threads_in = 0;
	sw_struct->worker_threads_out = 0;

	return sw_struct;
}


/**
 * This method resets the fields of the state swapping struct
 * 
 * @param A state_swapping_struct pointer
 */
void reset_state_swapping_struct(state_swapping_struct *old_state_swap_ptr) {


	state_swapping_struct *new_struct = alloc_state_swapping_struct();
	if (!__sync_bool_compare_and_swap(&state_swap_ptr, old_state_swap_ptr, new_struct)) {
		free (new_struct);
	}

}

void restore_context() {


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
 * This method swaps the commit horizon for a given lp
 * in order to do the state swapping operation
 * 
 * @param lp An lp index
 * @param reference_gvt The reference gvt for the current state swapping round
 */
void swap_state(int lp, simtime_t reference_gvt) {

	commit_horizon_to_save = LPS[lp]->commit_horizon_ts; ///save current commit horizon
	LPS[lp]->commit_horizon_ts = reference_gvt; /// commit horizon to be used in check_OnGVT

 #if VERBOSE == 1
	printf("REF GVT %f -- commit_horizon_ts %f -- commit_horizon_to_save %f\n", reference_gvt, LPS[lp]->commit_horizon_ts, commit_horizon_to_save);
 #endif
}


/**
 * This method restores the previously swapped commit horizon for a given lp
 * 
 * @param lp An lp index
 */
void restore_state(int lp) {

	LPS[lp]->commit_horizon_ts = commit_horizon_to_save; /// restore old commit horizon

 #if VERBOSE == 1
	printf("RESTORE COMMIT HORIZON %f\n", LPS[lp]->commit_horizon_ts);
 #endif

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
		

	if (!get_bit(cur_state_swap_ptr->lp_bitmap, cur_lp)) {

	  #if VERBOSE == 1
		printf("examining lp %d\n", cur_lp);
	  #endif 

		if(LPS[cur_lp]->commit_horizon_ts>0){

			set_bit(cur_state_swap_ptr->lp_bitmap, cur_lp);

			swap_state(cur_lp, cur_state_swap_ptr->reference_gvt);
			check_OnGVT(cur_lp);
			restore_state(cur_lp);	
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
void csr_routine(){

	int cur_lp;

	state_swapping_struct *cur_state_swap_ptr;

	while (1) {

		cur_state_swap_ptr = state_swap_ptr; 

		/// compute the reference gvt
		compute_reference_gvt(cur_state_swap_ptr);

	  #if VERBOSE == 1
		printf("gvt %f -- state_swap_ptr->reference_gvt %f\n", gvt, state_swap_ptr->reference_gvt);
	  #endif

		__sync_fetch_and_add(&cur_state_swap_ptr->worker_threads_in, 1);

		
		/// check if a lp was already locked in normal context
		if (haveLock(lp_lock[potential_locked_object])) { 
			set_bit(cur_state_swap_ptr->lp_bitmap, potential_locked_object);
			//output_collection(cur_state_swap_ptr, potential_locked_object, false);
		} //TODO commented just for now


		cur_lp = __sync_fetch_and_add(&cur_state_swap_ptr->counter_lp, -1);

		while(cur_lp >= 0) { 

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
			if (!get_bit(cur_state_swap_ptr->lp_bitmap, lp)) {

			  #if VERBOSE == 1
				printf("missing some lp %d\n", lp);
			  #endif

				output_collection(cur_state_swap_ptr, lp, true);

			}
		}
	  #endif

		__sync_fetch_and_add(&cur_state_swap_ptr->worker_threads_out, 1);

		reset_state_swapping_struct(cur_state_swap_ptr);
	  #if VERBOSE == 1
		print_state_swapping_struct(state_swap_ptr);
	  #endif

		if (!CSR_CONTEXT) break;

		/// syscall to restore normal context
		restore_context();

	} //end while(1)

	
	
}