#if IPI_HANDLE_INTERRUPT==1

#include <stdio.h>
#include <core.h>
#include <handle_interrupt.h>
#include <hpdcs_utils.h>
#include <queue.h>
#include <prints.h>

#include <jmp.h>
#include <preempt_counter.h>

#if DEBUG==1
#include <checks.h>
#endif

#if IPI_POSTING==1
#include <posting.h>
#endif

extern __thread cntx_buf cntx_loop;

msg_t* allocate_dummy_bound(int lp_idx){
	//alloc an event dummy. it won't be never connected to calqueue or localqueue,so it's possible change its state and it's possible set bound to dummy_bound with care.
	msg_t*dummy_bound=list_allocate_node_buffer_from_list(lp_idx, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
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
	#if DEBUG==1
	check_tie_breaker_not_zero(*tie_breaker);
	#endif
	*until_ts=LPS[lid]->dummy_bound->timestamp;
	*tie_breaker=LPS[lid]->dummy_bound->tie_breaker+1;
}

void make_LP_state_invalid_and_long_jmp(msg_t*restore_bound){
	#if REPORT==1 && IPI_POSTING==1 || IPI_SUPPORT==1
	if(LPS[current_lp]->state==LP_STATE_READY){
        statistics_post_lp_data(current_lp,STAT_EVENT_FORWARD_INTERRUPTED,1);
    }
    else{
       	statistics_post_lp_data(current_lp,STAT_EVENT_SILENT_INTERRUPTED,1);
    }
	#endif
	make_LP_state_invalid(restore_bound);
    wrap_long_jmp(&cntx_loop,CFV_ALREADY_HANDLED);
}


void change_bound_with_current_msg(){
	LPS[current_lp]->old_valid_bound=LPS[current_lp]->bound;
	LPS[current_lp]->dummy_bound->timestamp=current_msg->timestamp;
	LPS[current_lp]->dummy_bound->tie_breaker=current_msg->tie_breaker-1;
	LPS[current_lp]->bound=LPS[current_lp]->dummy_bound;
	#if DEBUG==1
	check_current_msg_is_in_future(current_lp);
	#endif
}

#if IPI_POSTING==1
void reset_info_and_change_bound(unsigned int lid,msg_t*event){
	#if DEBUG==1
	check_tie_breaker_not_zero(event->tie_breaker);
	#endif

	reset_priority_message(lid,LPS[lid]->priority_message);

	LPS[lid]->dummy_bound->state=ROLLBACK_ONLY;
	LPS[lid]->dummy_bound->timestamp=event->timestamp;
	LPS[lid]->dummy_bound->tie_breaker=event->tie_breaker-1;
	LPS[lid]->bound=LPS[lid]->dummy_bound;//modify bound,now priority message must be smaller than this bound
}

void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event){
	//modify until_ts and tie_breaker
	reset_info_and_change_bound(lid,event);
	change_dest_ts(lid,until_ts,tie_breaker);
}
#endif


#endif