#include "core.h"
#include <clock_constant.h>
#include <bitmap.h>
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
	__sync_bool_compare_and_swap(&state_swap_ptr->state_swap_flag, 0, 1);
    alarm(PERIOD_SIGNAL_S);    
}


/**
 * This method arms a signal and calls and alarm until the state_swap_flag has been set
 * 
 */
void signal_state_swapping() {

	signal(SIGALRM, handle_signal);
	alarm(PERIOD_SIGNAL_S);

	while(!state_swap_ptr->state_swap_flag);
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
	sw_struct->worker_threads = 0;

	return sw_struct;
}


/**
 * This method resets the fields of the state swapping struct
 * 
 * @param A state_swapping_struct pointer
 */
void reset_state_swapping_struct() {


	state_swapping_struct *new_struct = alloc_state_swapping_struct();
	__sync_bool_compare_and_swap(&state_swap_ptr, state_swap_ptr, new_struct);

}


/**
 * This method prints some fields of the state swapping structure, for debugging purposes
 * 
 * @param A state_swapping_struct pointer
 */
void print_state_swapping_struct(state_swapping_struct *ptr) {
	printf("state_swapping_struct->counter_lp %u --- state_swapping_struct->worker_threads %d --- state_swapping_struct->state_swap_flag %u ---- state_swapping_struct->reference_gvt %f \n", ptr->counter_lp, ptr->worker_threads, ptr->state_swap_flag, ptr->reference_gvt);
}


void swap_state(int lp, simtime_t reference_gvt) {

	//if (reference_gvt == 0) return;

	commit_horizon_to_save = LPS[lp]->commit_horizon_ts; ///save current commit horizon
	LPS[lp]->commit_horizon_ts = reference_gvt; /// commit horizon to be used in check_OnGVT

	printf("REF GVT %f -- commit_horizon_ts %f -- commit_horizon_to_save %f\n", reference_gvt, LPS[lp]->commit_horizon_ts, commit_horizon_to_save);
}


void restore_state(int lp) {

	LPS[lp]->commit_horizon_ts = commit_horizon_to_save; /// restore old commit horizon
	printf("RESTORE COMMIT HORIZON %f\n", LPS[lp]->commit_horizon_ts);

}

void csr_routine(){

	int cur_lp;
	bool to_release = true; //TODO: to be used in new trylock implementation

	while (1) {

		/// compute reference gvt
		__sync_bool_compare_and_swap(
							UNION_CAST(&(state_swap_ptr->reference_gvt), unsigned long long *),
							UNION_CAST(0,unsigned long long),
							UNION_CAST(local_gvt, unsigned long long)
				);

		__sync_fetch_and_add(&state_swap_ptr->worker_threads, 1);

		state_swap_ptr->counter_lp = n_prc_tot-1;
		cur_lp = state_swap_ptr->counter_lp;


		/*if (csr_mode && lp_lock[potential_locked_object]) = MakeWord(1, 1, tid) {
			if (!get_bit(state_swap_ptr->lp_bitmap, potential_locked_object)) {
				set_bit(state_swap_ptr->lp_bitmap, potential_locked_object);

				swap_state(potential_locked_object, state_swap_ptr->reference_gvt);
				check_OnGVT(potential_locked_object, LPS[potential_locked_object]->current_base_pointer);
				restore_state(potential_locked_object);

			}
		}*/

		while(cur_lp >= 0) {

		/**
		 * state output collection in sync scenario and in case of
		 * lps not locked in normal context when change of context is triggered
		**/
		  #if VERBOSE == 1
			printf("csr_routine lp %u\n", cur_lp);
		  #endif

			/// compute reference gvt another time to avoid it being zero
			__sync_bool_compare_and_swap(
								UNION_CAST(&(state_swap_ptr->reference_gvt), unsigned long long *),
								UNION_CAST(0,unsigned long long),
								UNION_CAST(local_gvt, unsigned long long)
					);
			
			if (!get_bit(state_swap_ptr->lp_bitmap, cur_lp) && tryLock(cur_lp)) {
				if (!get_bit(state_swap_ptr->lp_bitmap, cur_lp)) {
					
					if(LPS[cur_lp]->commit_horizon_ts>0){

						set_bit(state_swap_ptr->lp_bitmap, cur_lp);

						swap_state(cur_lp, state_swap_ptr->reference_gvt);
						check_OnGVT(cur_lp);
						restore_state(cur_lp);	
					}

				} ///end if get_bit

				if (to_release) unlock(cur_lp);
			} ///end if trylock

			cur_lp = __sync_fetch_and_add(&state_swap_ptr->counter_lp, -1);

		}///end while loop



		__sync_fetch_and_add(&state_swap_ptr->worker_threads, -1);
		if (state_swap_ptr->worker_threads == 1) {
			reset_state_swapping_struct(&state_swap_ptr);
		  #if VERBOSE == 1
			print_state_swapping_struct(state_swap_ptr);
		  #endif
		}

		if (!csr_mode) break;

		/// syscall to restore normal context
		//restore_context();

	} //end while(1)
	
}