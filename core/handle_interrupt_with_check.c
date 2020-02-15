#if HANDLE_INTERRUPT_WITH_CHECK==1

#include <handle_interrupt_with_check.h>
#include <checks.h>
#include <posting.h>
#include <hpdcs_utils.h>

#if DEBUG==1
__thread unsigned int nesting_zone_preemptable=0;
__thread unsigned int nesting_zone_unpreemptable=0;
#endif

void reset_nesting_counters(){
	nesting_zone_preemptable=0;
	nesting_zone_unpreemptable=0;
}
unsigned long enter_in_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable>=MAX_NESTING_ZONE_UNPREEMPTABLE){
		printf("error in enter_in_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable++;
	#endif
	return increment_preempt_counter();
}
unsigned long exit_from_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable==NO_NESTING){
		printf("error in exit_from_unpreemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable--;
	#endif
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
	return decrement_preempt_counter();
}

unsigned long exit_from_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable==NO_NESTING){
		printf("error in exit_from_preemptable_zone\n");
		gdb_abort;
	}
	nesting_zone_preemptable--;
	#endif
	return increment_preempt_counter();
}

void check_preemptability(){
	if(*preempt_count_ptr!=PREEMPT_COUNT_CODE_INTERRUPTIBLE){
		printf("code is unpreemptable\n");
		gdb_abort;
	}
}
void check_unpreemptability(){
	if(*preempt_count_ptr==PREEMPT_COUNT_CODE_INTERRUPTIBLE){
		printf("code is preemptable\n");
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