#if HANDLE_INTERRUPT_WITH_CHECK==1

#include <handle_interrupt_with_check.h>
#include <checks.h>
#include <posting.h>
#include <hpdcs_utils.h>

#if DEBUG==1
__thread unsigned int nesting_zone_preemptable=0;
__thread unsigned int nesting_zone_unpreemptable=0;


void reset_nesting_counters(){
	nesting_zone_preemptable=0;
	nesting_zone_unpreemptable=0;
}
#endif

void*default_handler(void*arg){
    //this function assume current_lp and current_msg are set.
    //this function works only with LP->state LP_STATE_READY or LP_STATE_SILENT_EXEC
    //set these variable from caller before call this function
    (void)arg;
    check_unpreemptability();
    if (LPS[current_lp]->state==LP_STATE_SILENT_EXEC){
        #if REPORT==1
        statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_SILENT,1);

        #endif
        #if VERBOSE >0
        printf("sync_check_silent\n");
        #endif
        msg_t*evt=get_best_LP_info_good(current_lp);
        if(evt!=NULL){
            if(current_msg==NULL){
            make_LP_state_invalid_and_long_jmp(LPS[current_lp]->old_valid_bound);
            }
            else{//current_msg not null
                insert_ordered_in_list(current_lp,(struct rootsim_list_node*)LPS[current_lp]->queue_in,LPS[current_lp]->last_silent_exec_evt,current_msg);
                statistics_post_lp_data(current_lp,STAT_EVENT_SILENT_INTERRUPTED,1);
                make_LP_state_invalid_and_long_jmp(list_prev(current_msg));
            }
        }
    }   
    else if (LPS[current_lp]->state==LP_STATE_READY){
        #if REPORT==1 
        statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_FORWARD,1);
        #endif
        #if VERBOSE >0
        printf("sync_check_forward\n");
        #endif
        msg_t*evt=get_best_LP_info_good(current_lp);
        if(evt!=NULL){
            //no need of insert current_msg
            statistics_post_lp_data(current_lp,STAT_EVENT_FORWARD_INTERRUPTED,1);
            make_LP_state_invalid_and_long_jmp(list_prev(current_msg));
        }
    }
    else{
        printf("LP_STATE not valid\n");
    }
    return NULL;//never reached
}

unsigned long enter_in_unpreemptable_zone(){
	unsigned int preemption_counter;
	#if DEBUG==1
	if(nesting_zone_unpreemptable>=MAX_NESTING_ZONE_UNPREEMPTABLE){
		printf("error in enter_in_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable++;
	#endif
	preemption_counter=increment_preempt_counter();
	if(preemption_counter==PREEMPT_COUNT_INIT)
		default_handler(NULL);
	return preemption_counter;
}
unsigned long exit_from_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable==NO_NESTING){
		printf("error in exit_from_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable--;
	#endif
	if(get_preemption_counter()==PREEMPT_COUNT_INIT)//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
		default_handler(NULL);
	return decrement_preempt_counter();
}

unsigned long enter_in_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable>=MAX_NESTING_ZONE_PREEMPTABLE){
		printf("error in enter_in_preemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_preemptable++;
	#endif
	if(get_preemption_counter()==PREEMPT_COUNT_INIT)
		default_handler(NULL);
	return decrement_preempt_counter();
}

unsigned long exit_from_preemptable_zone(){
	unsigned int preemption_counter;
	#if DEBUG==1
	if(nesting_zone_preemptable==NO_NESTING){
		printf("error in exit_from_preemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_preemptable--;
	#endif
	preemption_counter=increment_preempt_counter();
	if(preemption_counter==PREEMPT_COUNT_INIT)
		default_handler(NULL);
	return preemption_counter;
}

#if DEBUG==1
void check_preemptability(){
	//check if code enters at least once time in code preemptable
	if(nesting_zone_preemptable==0){
		printf("region of code unpreemptable in check_preemptability\n");
		gdb_abort;
	}
}
#endif

void check_unpreemptability(){
	if(get_preemption_counter()==PREEMPT_COUNT_CODE_INTERRUPTIBLE){
		printf("code is preemptable in check_unpreemptability function\n");
		gdb_abort;
	}
}

#if POSTING==1 //todo remove this macro,handle_interrupt_with_check include posting per defautl
void reset_info_and_change_bound(unsigned int lid,msg_t*event){
	#if DEBUG==1
	check_tie_breaker_not_zero(event->tie_breaker);
	#endif

	reset_priority_message(lid,LPS[lid]->priority_message);

	LPS[lid]->dummy_bound->state=ROLLBACK_ONLY;
	LPS[lid]->dummy_bound->timestamp=event->timestamp;
	LPS[lid]->dummy_bound->tie_breaker=event->tie_breaker;
	LPS[lid]->bound=LPS[lid]->dummy_bound;//modify bound,now priority message must be smaller than this bound
}

void reset_info_change_bound_and_change_dest_ts(unsigned int lid,simtime_t*until_ts,unsigned int*tie_breaker,msg_t*event){
	//modify until_ts and tie_breaker
	reset_info_and_change_bound(lid,event);
	change_dest_ts(lid,until_ts,tie_breaker);
}
#endif

#endif