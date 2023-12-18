#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <lp/lp.h>
#include <core.h>

#include <glo_alloc.h>
#include <lpm_alloc.h>

extern LP_state **LPS;
extern unsigned long long *sim_ended;
extern volatile unsigned int *lp_lock;

void LPs_metada_init(void) {
	unsigned int i;
	int lp_lock_ret;
	
	LPS         = glo_alloc(sizeof(void*) * pdes_config.nprocesses);
	sim_ended   = glo_alloc(LP_ULL_MASK_SIZE);
	lp_lock_ret = glo_memalign_alloc((void **)&lp_lock, CACHE_LINE_SIZE, pdes_config.nprocesses * CACHE_LINE_SIZE); 
			
	if(LPS == NULL || sim_ended == NULL || lp_lock_ret == 1){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();
	}
	
	for (i = 0; i < pdes_config.nprocesses; i++) {
		lp_lock[i*(CACHE_LINE_SIZE/4)] = 0;
		sim_ended[i/64] = 0;
		LPS[i] = lpm_alloc(sizeof(LP_state));
		LPS[i]->lid 					= i;
		LPS[i]->seed 					= i+1; //TODO;
		LPS[i]->state 					= LP_STATE_READY;
		LPS[i]->ckpt_period 			= pdes_config.ckpt_period;
		LPS[i]->from_last_ckpt 			= 0;
		LPS[i]->state_log_forced  		= false;
		LPS[i]->current_base_pointer 	= NULL;
		LPS[i]->queue_in 				= new_list(i, msg_t);
		LPS[i]->bound 					= NULL;
		LPS[i]->queue_states 			= new_list(i, state_t);
		LPS[i]->mark 					= 0;
		LPS[i]->epoch 					= 1;
		LPS[i]->num_executed_frames		= 0;
		LPS[i]->until_clean_ckp			= 0;
		bzero(&LPS[i]->local_index, sizeof(LPS[i]->local_index));
		LPS[i]->wt_binding = UNDEFINED_WT;

		/* FIELDS FOR COMPUTING AVG METRICS	*/
		LPS[i]->ema_ti					= 0.0;
		LPS[i]->ema_granularity			= 0.0;
		LPS[i]->migration_score			= 0.0;
		

		LPS[i]->ema_silent_event_granularity = 0.0;
		LPS[i]->ema_take_snapshot_time		 = 0.0;
		LPS[i]->ema_rollback_probability	 = 0.0;
	}
	
	for(; i<(LP_BIT_MASK_SIZE) ; i++)
		end_sim(i);

	
}