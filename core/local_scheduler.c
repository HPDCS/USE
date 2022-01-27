#include <lp_local_struct.h>
#include <local_index/local_index.h>
#include <queue.h>


#define CURRENT_BINDING_SIZE 1

__thread lp_local_struct thread_locked_binding[CURRENT_BINDING_SIZE];
__thread lp_local_struct thread_unlocked_binding[CURRENT_BINDING_SIZE];

void local_binding_init(){
	int i;
	for(i = 0; i < CURRENT_BINDING_SIZE; i++){
		thread_locked_binding[i].lp = 0;
		thread_locked_binding[i].hotness = 0;
		thread_locked_binding[i].distance_curr_evt_from_gvt = 0;
		thread_locked_binding[i].distance_last_ooo_from_gvt = 0;
		thread_locked_binding[i].evicted = true;
	}
}

static void local_binding_push(int lp){
	// STEP 1. release lock on evicted lp
	if(!thread_locked_binding[0].evicted) unlock(thread_locked_binding[0].lp);

	process_input_channel(LPS[lp]); // Fix input channel
	thread_locked_binding[0].lp = lp;
	thread_locked_binding[0].hotness 					= -1;
	thread_locked_binding[0].distance_curr_evt_from_gvt = -1;
	thread_locked_binding[0].distance_last_ooo_from_gvt = -1;
	thread_locked_binding[0].evicted = false;
	if(LPS[lp]->actual_index_top != NULL){
		msg_t *next_evt = LPS[lp]->actual_index_top->payload;
		thread_locked_binding[0].distance_curr_evt_from_gvt = next_evt->timestamp - current_lvt;
	}
}

static void local_binding_update(){
	int i;
	for(i = 0; i < CURRENT_BINDING_SIZE; i++){
		if(thread_locked_binding[i].evicted == false){
			int cur_lp = thread_locked_binding[i].lp;
			process_input_channel(LPS[cur_lp]);
		}
	}
}


int local_fetch(){
	local_binding_update();
	return 0;
}