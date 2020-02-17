#if HANDLE_INTERRUPT==1

#include <stdio.h>
#include <handle_interrupt.h>
#include <hpdcs_utils.h>
#include <timer.h>

#if DEBUG==1
#include <checks.h>
#endif

#if HANDLE_INTERRUPT_WITH_CHECK==1
#include <handle_interrupt_with_check.h>
#endif

#if IPI_SUPPORT==1
#include <ipi.h>
#include <lp_stats.h>

#endif

extern __thread cntx_buf cntx_loop;

void end_exposition_of_current_event(msg_t*event){
	#if HANDLE_INTERRUPT_WITH_CHECK==1
	check_unpreemptability();
	#endif
	if(event!=NULL){
		clock_timer exposition_timer = clock_timer_value(event->evt_start_time);
		#if IPI_SUPPORT==1
		store_lp_stats((lp_evt_stats *) LPS[current_lp]->lp_statistics, event->execution_mode, event->type, exposition_timer);
		#endif
		#if REPORT==1
		if(event->execution_mode==LP_STATE_READY){
			statistics_post_lp_data(current_lp,STAT_EVENT_EXPOSITION_FORWARD_LP,1);
			statistics_post_lp_data(current_lp,STAT_CLOCK_EXPOSITION_FORWARD_TOT_LP,exposition_timer);
		}
		else{
			statistics_post_lp_data(current_lp,STAT_EVENT_EXPOSITION_SILENT_LP,1);
			statistics_post_lp_data(current_lp,STAT_CLOCK_EXPOSITION_SILENT_TOT_LP,exposition_timer);
		}
		#endif
		//reset exposition of current_msg in execution
		LPS[current_lp]->msg_curr_executed=NULL;
	}
}

void start_exposition_of_current_event(msg_t*event){
	#if HANDLE_INTERRUPT_WITH_CHECK==1
	check_unpreemptability();
	#endif
	if(event!=NULL){
		clock_timer_start(event->evt_start_time);//event starting time sampled just before updating preemption_counter
		event->execution_mode=LPS[current_lp]->state;
		//start exposition of current_msg
		LPS[current_lp]->msg_curr_executed=event;
	}
}

msg_t* allocate_dummy_bound(unsigned int lp_idx){
	//alloc an event dummy. it won't be never connected to calqueue or localqueue,so it's possible change its state and it's possible set bound to dummy_bound with care.
	msg_t*dummy_bound = allocate_event(lp_idx);
	dummy_bound->sender_id 		= -1;//don't care
	dummy_bound->receiver_id 	= lp_idx;
	dummy_bound->timestamp 		= 0.0;
	dummy_bound->tie_breaker	= 1;
	dummy_bound->type 			= INIT;//don't care
	dummy_bound->state 			= NEW_EVT;
	dummy_bound->father 		= NULL;//don't care
	dummy_bound->epoch 		= 0;//don't care
	dummy_bound->monitor 		= 0x0;//don't care
	list_node_clean_by_content(dummy_bound);
	return dummy_bound;
}

bool dummy_bound_is_corrupted(int lp_idx){
	//return true if dummy_bound is invalid,false otherwise
	//dummy bound is invalid if contains fileds'values different from its initialization
	//expect filed timestamp,tie_breaker and state
	msg_t*dummy_bound=LPS[lp_idx]->dummy_bound;

	return(
		(dummy_bound==NULL)
	|| (dummy_bound->receiver_id != lp_idx)
	|| (dummy_bound->type != INIT)
	|| ( (dummy_bound->state != NEW_EVT) && (dummy_bound->state != ROLLBACK_ONLY))
	|| (dummy_bound->father != NULL)
	|| (dummy_bound->epoch != 0)
	|| (dummy_bound->monitor != 0x0)
	);
}

bool bound_is_corrupted(int lp_idx){
	//bound is corrupted if bound==dummy_bound,because in this case bound is not last event executed
	if(LPS[lp_idx]->bound==LPS[lp_idx]->dummy_bound)
		return true;
	return false;
}

void make_LP_state_invalid(msg_t*restore_bound){
	LPS[current_lp]->msg_curr_executed=NULL;
    LPS[current_lp]->LP_state_is_valid=false;//invalid state
    LPS[current_lp]->dummy_bound->state=NEW_EVT;
    LPS[current_lp]->state=LP_STATE_READY;//restore LP_state
    LPS[current_lp]->bound=restore_bound;
    LPS[current_lp]->old_valid_bound=NULL;
}

void change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker){
	*until_ts=LPS[lid]->dummy_bound->timestamp;
	*tie_breaker=LPS[lid]->dummy_bound->tie_breaker;
}

void make_LP_state_invalid_and_long_jmp(msg_t*restore_bound){
	#if DEBUG==1 && HANDLE_INTERRUPT_WITH_CHECK==1
	reset_nesting_counters();
	#endif
	make_LP_state_invalid(restore_bound);
    wrap_long_jmp(&cntx_loop,CFV_ALREADY_HANDLED);
}


void change_bound_with_current_msg(){
	#if DEBUG==1
	check_tie_breaker_not_zero(current_msg->tie_breaker);
	#endif

	LPS[current_lp]->old_valid_bound=LPS[current_lp]->bound;
	LPS[current_lp]->dummy_bound->timestamp=current_msg->timestamp;
	LPS[current_lp]->dummy_bound->tie_breaker=current_msg->tie_breaker-1;
	LPS[current_lp]->bound=LPS[current_lp]->dummy_bound;
	#if DEBUG==1
	check_current_msg_is_in_future(current_lp);
	#endif
}


#endif