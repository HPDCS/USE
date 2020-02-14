#if DEBUG==1

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <core.h>
#include <timer.h>
#include <list.h>
#include <state.h>
#include <dymelor.h>
#include <statistics.h>
#include <queue.h>
#include <hpdcs_utils.h>
#include <prints.h>

#if POSTING==1
#include <posting.h>
#endif

#if HANDLE_INTERRUPT==1
#include <handle_interrupt.h>
#endif

#if PREEMPT_COUNTER==1
#include <preempt_counter.h>
#endif
extern __thread unsigned long long current_evt_state;
extern __thread void* current_evt_monitor;


#if HANDLE_INTERRUPT==1
void check_after_rollback(){
	if(current_msg->timestamp<LPS[current_lp]->old_valid_bound->timestamp
	|| ((current_msg->timestamp==LPS[current_lp]->old_valid_bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->old_valid_bound->tie_breaker)) ){
		printf("event in past\n");
		gdb_abort;
	}
	if(LPS[current_lp]->LP_state_is_valid==false){
		printf("LP state invalid\n");
		gdb_abort;
	}
	if(!bound_is_corrupted(current_lp)){
		printf("bound not corrupted thread loop\n");
		gdb_abort;
	}
	if(LPS[current_lp]->dummy_bound->state==ROLLBACK_ONLY){
		printf("dummy bound is ROLLBACK_ONLY thread loop\n");
		gdb_abort;
	}
}
#endif

void check_CFV_ALREADY_HANDLED(){
	if(LPS[current_lp]->state != LP_STATE_READY){
		printf("LP state is not ready\n");
		gdb_abort;
	}
	#if PREEMPT_COUNTER==1
	if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
				printf("preempt counter is not INIT in CFV_ALREADY_HANDLED\n");
				gdb_abort;
	}
	#endif

	#if HANDLE_INTERRUPT==1
	if(LPS[current_lp]->dummy_bound->state!=NEW_EVT){
		printf("dummy bound is not NEW_EVT CFV_ALREADY_HANDLED\n");
		gdb_abort;
	}
	if(LPS[current_lp]->msg_curr_executed!=NULL){
		printf("msg_curr_executed different than NULL in CFV_ALREADY_HANDLED\n");
		gdb_abort;
	}
	
	if(dummy_bound_is_corrupted(current_lp)){
		printf("dummy_bound is corrupted\n");
		print_event(LPS[current_lp]->dummy_bound);
		gdb_abort;
	}
	if(bound_is_corrupted(current_lp)){
		printf("bound is corrupted\n");
		gdb_abort;
	}
	if(LPS[current_lp]->old_valid_bound!=NULL){
		printf("old valid bound not NULL CFV_ALREADY_HANDLED\n");
		gdb_abort;
	}
	#endif
}
void check_thread_loop_before_fetch(){
	#if PREEMPT_COUNTER==1
	if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
		printf("preempt counter is not INIT before simulation loop\n");
		gdb_abort;
	}
	#endif
	for(unsigned int lp_idx=0;lp_idx<n_prc_tot;lp_idx++){
		if (haveLock(lp_idx)){
			printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
			gdb_abort;
		}
	}
}
void check_CFV_INIT(){
	#if PREEMPT_COUNTER==1
	if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
				printf("preempt counter is not INIT in CFV_INIT\n");
				gdb_abort;
	}
	#endif
}
void check_CFV_TO_HANDLE(){
	if (!haveLock(current_lp)){
				printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
				gdb_abort;
	}
	#if PREEMPT_COUNTER==1
	if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
			printf("interrupt code not interruptible\n");
			gdb_abort;
	}
	#endif
	#if HANDLE_INTERRUPT==1
	if(LPS[current_lp]->old_valid_bound==NULL){
		printf("old_valid_bound is NULL CFV_TO_HANDLE\n");
		gdb_abort;
	}
	#endif
}

void check_thread_loop_after_fetch(){
	if (current_msg->tie_breaker==0){
		printf("fetched event with tie_breaker zero\n");
		print_event(current_msg);
		gdb_abort;
	}
	if(LPS[current_lp]->state != LP_STATE_READY){
		printf("LP state is not ready\n");
		gdb_abort;
	}
	#if HANDLE_INTERRUPT==1
	if(LPS[current_lp]->dummy_bound->state!=NEW_EVT){
		printf("dummy bound is not NEW_EVT\n");
		gdb_abort;
	}
	if(dummy_bound_is_corrupted(current_lp)){
		printf("dummy_bound is corrupted\n");
		print_event(LPS[current_lp]->dummy_bound);
		gdb_abort;
	}
	if(bound_is_corrupted(current_lp)){
		printf("bound is corrupted\n");
		gdb_abort;
	}
	if(LPS[current_lp]->msg_curr_executed!=NULL){
		printf("msg_curr_executed different than NULL thread_loop\n");
		gdb_abort;
	}
	if(LPS[current_lp]->old_valid_bound!=NULL){
			printf("old valid bound not NULL thread loop\n");
			gdb_abort;
	}
	#endif
	if( (current_evt_state!=ANTI_MSG) && (current_evt_state!= EXTRACTED) ){
		printf("current_evt_state=%lld\n",current_evt_state);
		gdb_abort;
	}
	if(!haveLock(current_lp)){//DEBUG
		printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
	}
	if(current_evt_state == ANTI_MSG && current_evt_monitor == (void*) 0xba4a4a) {
		printf(RED("TL: ho trovato una banana!\n")); print_event(current_msg);
		gdb_abort;
	}
}
void check_tie_breaker_not_zero(unsigned int tie_breaker){
	if(tie_breaker==0){
		printf("tie breaker is zero in silent execution\n");
		gdb_abort;
	}
}
void check_current_msg_is_in_future(unsigned int lid){
	if((LPS[lid]->bound!=NULL) && (current_msg!=NULL) && (current_msg->timestamp<LPS[lid]->bound->timestamp
		|| ((current_msg->timestamp==LPS[lid]->bound->timestamp) && (current_msg->tie_breaker<=LPS[lid]->bound->tie_breaker)))  ){
		printf("execution in progress of event in past in mode SILENT_EXECUTION\n");
		gdb_abort;
	}
}

void check_silent_execution(unsigned short int old_state){
	if(old_state!=LP_STATE_ONGVT && old_state!=LP_STATE_ROLLBACK){
		printf("invalid execution mode of LP in silent_execution function,execution_mode=%d,tid=%d\n",old_state,tid);
		gdb_abort;
	}
}

void check_stop_rollback(unsigned short int old_state,unsigned int lid){
	if(old_state == LP_STATE_ONGVT){
		printf("event will be executed because we are in LP_STATE_ON_GVT but is not valid\n");
		gdb_abort;
	}
	#if HANDLE_INTERRUPT==1
	if(LPS[lid]->old_valid_bound==NULL){
		printf("old_valid_bound is NULL in silent execution\n");
		gdb_abort;
	}
	#else
	(void)lid;
	#endif
}

void check_epoch_and_frame(unsigned int last_frame_event,unsigned int local_next_frame_event,unsigned int last_epoch_event,unsigned int local_next_epoch_event){
	if(last_frame_event!=local_next_frame_event-1){
		printf("not consecutive frames in silent_execution,frame_last_event=%d,frame_next_event=%d\n",last_frame_event,local_next_frame_event);
		gdb_abort;
	}
	if(last_epoch_event>local_next_epoch_event){
		printf("epoch number not strictly greater in silent_execution,epoch_last_event=%d,epoch_next_event=%d\n",last_epoch_event,local_next_epoch_event);
		gdb_abort;
	}
}
void check_events_state(unsigned short int old_state,msg_t*evt,msg_t*local_next_evt){
	if( ((local_next_evt->state & EXTRACTED)!=EXTRACTED) || ((evt->state & EXTRACTED)!=EXTRACTED)){
		printf("event in silent_execution is not EXTRACTED or ANTI_MSG\n");
		print_event(local_next_evt);
		print_event(evt);
		gdb_abort;
	}
	if( (old_state==LP_STATE_ONGVT) && local_next_evt->monitor!=(void*)0x5AFE){
		printf("LP_STATE_ONGVT but event is not been committed\n");
		gdb_abort;
	}
}

void check_queue_deliver_msgs(){
    if(LPS[current_lp]->state != LP_STATE_READY){
            printf("LP state is not ready in queue_deliver_msgs\n");
            gdb_abort;
    }
    #if HANDLE_INTERRUPT==1
    if(current_msg==NULL){
                printf("current_msg NULL queuedeliver_msgs\n");
                gdb_abort;
    }
    #endif
}

void check_ScheduleNewEventFuture(){
	if(LPS[current_lp]->state != LP_STATE_READY){
            printf("LP state is not ready ScheduleNewEvent\n");
            gdb_abort;
        }
        #if HANDLE_INTERRUPT==1
        if(current_msg==NULL){
        	printf("current msg is NULL in ScheduleNewEvent,LP_STATE_READY\n");
        	gdb_abort;
        }
        #endif
    	if((LPS[current_lp]->bound!=NULL) && (current_msg->timestamp<LPS[current_lp]->bound->timestamp
		|| ((current_msg->timestamp==LPS[current_lp]->bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->bound->tie_breaker)))  ){
			printf("execution in progress of event in past in not mode SILENT_EXECUTION in ScheduleNewEvent function\n");
			gdb_abort;
		}
		if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)) {
				printf("event in future is not connected to localqueue\n");
				gdb_abort;
		}
}
void check_ScheduleNewEventPast(){
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
            printf("LP state is not LP_STATE_SILENT_EXEC ScheduleNewEvent\n");
            gdb_abort;
    }	
	if((LPS[current_lp]->bound!=NULL) && (current_msg!=NULL) && (current_msg->timestamp<LPS[current_lp]->bound->timestamp
		|| ((current_msg->timestamp==LPS[current_lp]->bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->bound->tie_breaker)))  ){
			printf("execution in progress of event in past in mode SILENT_EXECUTION in ScheduleNewEvent function\n");
			gdb_abort;
	}
}


void check_CFV_TO_HANDLE_current_msg_null(){
	if(LPS[current_lp]->state!=LP_STATE_SILENT_EXEC){
		printf("current_msg is NULL and LP_STATE is different from LP_STATE_SILENT_EXEC\n");
		gdb_abort;
	}
}
void check_CFV_TO_HANDLE_past(){
	if((LPS[current_lp]->bound!=NULL) && (current_msg->timestamp<LPS[current_lp]->bound->timestamp
					|| ((current_msg->timestamp==LPS[current_lp]->bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->bound->tie_breaker)))  ){
						printf("execution in progress of event in past in mode SILENT_EXECUTION\n");
						gdb_abort;
	}
}
void check_CFV_TO_HANDLE_future(){
	if( (current_msg->timestamp <LPS[current_lp]->bound->timestamp)
		|| ((current_msg->timestamp ==LPS[current_lp]->bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->bound->tie_breaker) )){
		printf("event not in future,%lx,%lx,tid=%d\n",(unsigned long)current_msg,(unsigned long)LPS[current_lp]->bound,tid);
		printf("ts=%lf,tb=%d,ts=%lf,tb=%d\n", current_msg->timestamp,current_msg->tie_breaker,LPS[current_lp]->bound->timestamp,LPS[current_lp]->bound->tie_breaker);
		gdb_abort;
	}
	if(current_msg->monitor==(void*)0x5AFE){
		printf("interrupted event already committed\n");
		gdb_abort;
	}
	if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)) {
		printf("event in future is not connected to localqueue\n");
		gdb_abort;
	}
	if(current_msg==NULL){
		printf("current_msg NULL CFV_TO_HANDLE\n");
		gdb_abort;
	}
}

void check_thread_loop_after_executeEvent(){
	if(_thr_pool._thr_pool_count!=0){//thread pool count must be 0 after queue_deliver_msgs
        printf("not empty pool count,tid=%d\n",tid);
        gdb_abort;
    }
#if POSTING==1
    for(unsigned int i=0;i<MAX_THR_HASH_TABLE_SIZE;i++){
        if(_thr_pool.collision_list[i]!=NULL){
            printf("not empty collision list,tid=%d\n",tid);
            gdb_abort;
        }
    }
#endif
	if((unsigned long long)current_msg->node & 0x1){
			printf(RED("B - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
			print_event(current_msg);
			gdb_abort;				
	}
	#if HANDLE_INTERRUPT==1
	if(LPS[current_lp]->old_valid_bound->frame != LPS[current_lp]->num_executed_frames-1){
			printf("invalid frame number in event,frame_event=%d,frame_LP=%d\n",LPS[current_lp]->old_valid_bound->frame,LPS[current_lp]->num_executed_frames);
			gdb_abort;
	}
	if(LPS[current_lp]->old_valid_bound->epoch > current_msg->epoch){
		printf("invalid epoch number in event,epoch_bound=%d,epoch_current_msg=%d\n",LPS[current_lp]->old_valid_bound->epoch,current_msg->epoch);
		gdb_abort;
	}
	#endif
}

#endif