#if HANDLE_INTERRUPT_WITH_CHECK==1

#include <handle_interrupt_with_check.h>
#include <checks.h>
#include <posting.h>
#include <hpdcs_utils.h>

#if DECISION_MODEL==1 && REPORT==1
#include <lp_stats.h>
#endif

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
    #if DEBUG==1
    check_unpreemptability();
    #endif
    if (LPS[current_lp]->state==LP_STATE_SILENT_EXEC){
        #if REPORT==1
        statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_SILENT_LP,1);

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
            	#if DECISION_MODEL==1 && REPORT==1
                clock_timer time_evt_interrupted=clock_timer_value(LPS[current_lp]->msg_curr_executed->evt_start_time);
                statistics_post_lp_data(current_lp,STAT_EVENT_EXPOSITION_SILENT_SYNCH_INTERRUPTED_LP,1);
                statistics_post_lp_data(current_lp,STAT_CLOCK_EXPOSITION_SILENT_SYNCH_INTERRUPTED_LP,time_evt_interrupted);
                clock_timer time_gained;
				clock_timer actual_mean=get_actual_mean(current_lp,LP_STATE_SILENT_EXEC,LPS[current_lp]->msg_curr_executed->type);
				//printf("silent synch actual mean=%llu,time_evt_interrupted=%llu\n",actual_mean,time_evt_interrupted);
				if(actual_mean > time_evt_interrupted)
					time_gained=actual_mean - time_evt_interrupted;
				else
					time_gained=0;
				statistics_post_lp_data(current_lp,STAT_CLOCK_RESIDUAL_TIME_SILENT_SYNCH_GAINED_LP,time_gained);
				
				#endif

                insert_ordered_in_list(current_lp,(struct rootsim_list_node*)LPS[current_lp]->queue_in,LPS[current_lp]->last_silent_exec_evt,current_msg);
                make_LP_state_invalid_and_long_jmp(list_prev(current_msg));
            }
        }
    }   
    else if (LPS[current_lp]->state==LP_STATE_READY){
        #if REPORT==1 
        statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_FORWARD_LP,1);
        #endif
        #if VERBOSE >0
        printf("sync_check_forward\n");
        #endif
        msg_t*evt=get_best_LP_info_good(current_lp);
        if(evt!=NULL){
        	#if DECISION_MODEL==1 && REPORT==1
            //no need of insert current_msg in localqueue
            clock_timer time_evt_interrupted=clock_timer_value(LPS[current_lp]->msg_curr_executed->evt_start_time);
            statistics_post_lp_data(current_lp,STAT_EVENT_EXPOSITION_FORWARD_SYNCH_INTERRUPTED_LP,1);
            statistics_post_lp_data(current_lp,STAT_CLOCK_EXPOSITION_FORWARD_SYNCH_INTERRUPTED_LP,time_evt_interrupted);
            
            clock_timer time_gained;
			clock_timer actual_mean=get_actual_mean(current_lp,LP_STATE_READY,LPS[current_lp]->msg_curr_executed->type);
			//printf("forward synch actual mean=%llu,time_evt_interrupted=%llu\n",actual_mean,time_evt_interrupted);
			if(actual_mean > time_evt_interrupted)
				time_gained=actual_mean - time_evt_interrupted;
			else
				time_gained=0;
			statistics_post_lp_data(current_lp,STAT_CLOCK_RESIDUAL_TIME_FORWARD_SYNCH_GAINED_LP,time_gained);
            #endif
            make_LP_state_invalid_and_long_jmp(list_prev(current_msg));
        }
    }
    else{
        printf("LP_STATE not valid\n");
        gdb_abort;
    }
    return NULL;//never reached
}

void enter_in_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable>=MAX_NESTING_ZONE_UNPREEMPTABLE){
		printf("error in enter_in_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable++;
	#endif
	increment_preempt_counter();
}
void  exit_from_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable==NO_NESTING){
		printf("error in exit_from_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable--;
	#endif
	#if SYNCH_CHECK==1
	//counter can became 0 from 1,before decrement do_check
	if(get_preemption_counter()==PREEMPT_COUNT_INIT)//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
		default_handler(NULL);
	#endif
	decrement_preempt_counter();
}

void  enter_in_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable>=MAX_NESTING_ZONE_PREEMPTABLE){
		printf("error in enter_in_preemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_preemptable++;
	#endif
	#if SYNCH_CHECK==1
	//counter can became 0 from 1,before decrement do_check
	if(get_preemption_counter()==PREEMPT_COUNT_INIT)
		default_handler(NULL);
	#endif
	decrement_preempt_counter();
}

void exit_from_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable==NO_NESTING){
		printf("error in exit_from_preemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_preemptable--;
	#endif
	increment_preempt_counter();
}

#if DEBUG==1
void check_preemptability(){
	//check if code enters at least once time in code preemptable
	if(nesting_zone_preemptable==0){
		printf("region of code unpreemptable in check_preemptability\n");
		gdb_abort;
	}
}

void check_unpreemptability(){
	if(get_preemption_counter()==PREEMPT_COUNT_CODE_INTERRUPTIBLE){
		printf("code is preemptable in check_unpreemptability function\n");
		gdb_abort;
	}
}
#endif

#endif