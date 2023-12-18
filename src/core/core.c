#include <os/numa.h>
#include <os/thread.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>

#include <ROOT-Sim.h>
#include <numerical/numerical.h>
#include <state.h>

#include <statistics/statistics.h>

#include <lp/lp.h>
#include <lp/mm/checkpoints/autockpt.h>
#include <lp/mm/allocator/dymelor.h>
#include <lp/mm/segment/buddy.h>

#include "core.h"
#include "queue.h"
#include "lookahead.h"

#include <utils/prints.h>
#include <utils/timer.h>

#include <utmpx.h>
#include <gvt.h>


#include <numa_migration_support.h>
#include "metrics_for_lp_migration.h"
#include <gvt.h>

#include <scheduler/nb_calqueue.h>
#include <scheduler/local/local_index/local_index.h>
#include <scheduler/local/local_scheduler.h>
#include <scheduler/local/metrics_for_window.h>

#include "clock_constant.h"
#include "state_swapping.h"


#include <glo_alloc.h>
#include <lpm_alloc.h>


#define MAIN_PROCESS		0 //main process id
#define PRINT_REPORT_RATE	1000000000000000


unsigned int start_periodic_check_ongvt = 0;

extern clock_timer simulation_clocks;
extern timer exec_time;

__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;
__thread unsigned long long evt_count = 0;
__thread unsigned long long current_evt_state = 0;
__thread void* current_evt_monitor = 0x0;
__thread struct __bucket_node *current_node = 0x0;
__thread unsigned int last_checked_lp = 0;
__thread unsigned int check_ongvt_period = 0;
__thread unsigned int current_numa_node;
__thread unsigned int current_cpu;
__thread int __event_from = 0;

#if REPORT == 1
__thread clock_timer main_loop_time,		//total execution time on single core
				fetch_timer,				//time used to correctly fetch an event 		
				event_processing_timer,		//time used to process a safe event 
				queue_op,
				rollback_timer
#if REVERSIBLE == 1
				,stm_event_processing_time
				,stm_safety_wait
#endif
				;
#endif



volatile unsigned int ready_wt = 0;

/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1;

bool sim_error = false;

//void **states;
LP_state **LPS = NULL;

//used to check termination conditions
volatile bool stop = false;
volatile bool stop_timer = false;
volatile unsigned int lp_inizialized = 0;
pthread_t sleeper;//pthread_t p_tid[number_of_threads];//

unsigned long long *sim_ended;

numa_struct **numa_state;			/// state of every numa node
pthread_mutex_t numa_balance_check_mtx; /// mutex for checking numa unbalance
unsigned int num_numa_nodes;
bool numa_available_bool;
unsigned int rounds_before_unbalance_check; /// number of window management rounds before numa unbalance check
clock_timer numa_rebalance_timer; /// timer to check for periodic numa rebalancing
size_t node_size_msg_t;
size_t node_size_state_t;
unsigned int cores_on_numa[N_CPU];


void * do_sleep(){
	if(pdes_config.timeout != 0)
		printf("The simulation will last %d seconds\n", pdes_config.timeout);
	sleep(pdes_config.timeout);
	if(pdes_config.timeout != 0)
		stop_timer = true;
	pthread_exit(NULL);
}

void nodes_init(){
		size_t tmp_size;
		unsigned int i;
		
		i = 0;
		tmp_size = sizeof(struct rootsim_list_node) + sizeof(msg_t);//multiple of 64
		while((++i)*CACHE_LINE_SIZE < tmp_size);
		node_size_msg_t = (i)*CACHE_LINE_SIZE;
		
		i = 0;
		tmp_size = sizeof(struct rootsim_list_node) + sizeof(state_t);//multiple of 64
		while((++i)*CACHE_LINE_SIZE < tmp_size);
		node_size_state_t = (i)*CACHE_LINE_SIZE;
}


void pin_lps_on_numa_nodes_init() {
	numa_state = glo_alloc(num_numa_nodes * sizeof(numa_struct *));
	for (unsigned int i=0; i < num_numa_nodes; i++) {
		numa_state[i] = glo_alloc(sizeof(numa_struct));
		alloc_numa_state(numa_state[i]);
		numa_state[i]->numa_id = i;
	}

	unsigned cur_lp = 0;
	for(unsigned int j=0;j<num_numa_nodes;j++){
		for(unsigned int i=0;i<pdes_config.nprocesses/num_numa_nodes;i++) {
			set_bit(numa_state[j]->numa_binding_bitmap, cur_lp);
			LPS[cur_lp]->numa_node 				= j;
		  #if VERBOSE == 1
			printf("INI NUMA %d LP %d b:%d\n",  j, cur_lp, get_bit(numa_state[j]->numa_binding_bitmap, cur_lp));
		  #endif
			cur_lp++;
		}
	}
	for(;cur_lp<pdes_config.nprocesses;cur_lp++){
		set_bit(numa_state[num_numa_nodes-1]->numa_binding_bitmap, cur_lp);
		LPS[cur_lp]->numa_node 				= num_numa_nodes-1;
	  #if VERBOSE == 1
		printf("INI NUMA %d LP %d b:%d\n",  num_numa_nodes-1, cur_lp, get_bit(numa_state[num_numa_nodes-1]->numa_binding_bitmap, cur_lp));
	  #endif
	}

}


//TODO: use a bitmap
//Note: called at end execution
bool check_termination(void) {
	unsigned int i;
	
	
	for (i = 0; i < LP_MASK_SIZE; i++) {
		if(sim_ended[i] != ~(0ULL))
			return false; 
	}
	
	return true;
}


//#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event),event, event->node);


void check_OnGVT(unsigned int lp_idx, simtime_t ts, unsigned int tb){
	unsigned int old_state;
	current_lp = lp_idx;
	LPS[lp_idx]->until_ongvt = 0;
	
	//if(!is_end_sim(lp_idx))
    {
		//Restore state on the commit_horizon
		old_state = LPS[lp_idx]->state;
		LPS[lp_idx]->state = LP_STATE_ONGVT;
		//printf("%d- BUILD STATE FOR %d TIMES GVT START LVT:%f commit_horizon:%f\n", current_lp, LPS[current_lp]->until_ongvt, LPS[current_lp]->current_LP_lvt, LPS[current_lp]->commit_horizon_ts);
		rollback(lp_idx, ts, tb);
        //statistics_post_lp_data(lp_idx, STAT_ROLLBACK, 1);
		//printf("%d- BUILD STATE FOR %d TIMES GVT END\n", current_lp );
		
		//printf("[%u]ONGVT LP:%u TS:%f TB:%llu\n", tid, lp_idx,LPS[lp_idx]->commit_horizon_ts, LPS[lp_idx]->commit_horizon_tb);
		if(OnGVT(lp_idx, LPS[lp_idx]->current_base_pointer)){
			//printf("Simulation endend on LP:%u\n", lp_idx);
			start_periodic_check_ongvt = 1;
			end_sim(lp_idx);
		}
		// Restore state on the bound
		LPS[lp_idx]->state = LP_STATE_ROLLBACK;
		//printf("%d- BUILD STATE AFTER GVT START LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
        //statistics_post_lp_data(lp_idx, STAT_ROLLBACK, 1);
		rollback(lp_idx, INFTY, UINT_MAX);
		//printf("%d- BUILD STATE AFTER GVT END LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		LPS[lp_idx]->state = old_state;

		statistics_post_lp_data(lp_idx, STAT_ONGVT, 1);
	}
}



void round_check_OnGVT(){
	unsigned int start_from = (pdes_config.nprocesses / pdes_config.ncores) * tid;
	unsigned int curent_ck = start_from;
	
	
	//printf("OnGVT: la coda è vuota!\n");
	
	do{
		if(!is_end_sim(curent_ck)){
			if(tryLock(curent_ck)){
				//commit_horizon_ts = 5000;
				//commit_horizon_tb = 100;
				if(LPS[curent_ck]->commit_horizon_ts>0){
//					printf("[%u]ROUND ONGVT LP:%u TS:%f TB:%llu\n", tid, curent_ck,LPS[curent_ck]->commit_horizon_ts, LPS[curent_ck]->commit_horizon_tb);
	
					check_OnGVT(curent_ck, LPS[curent_ck]->commit_horizon_ts, LPS[curent_ck]->commit_horizon_tb);
				}
				unlock(curent_ck);
				break;
			}
		}
		curent_ck = (curent_ck + 1) % pdes_config.nprocesses;
	}while(curent_ck != start_from);
}

void init_simulation(unsigned int thread_id){
    clock_timer init_timer;
    clock_timer_start(init_timer);
	tid = thread_id;
	
#if REVERSIBLE == 1
	reverse_init(REVWIN_SIZE);
#endif

	
	// Initialize the set ??
	unsafe_set_init();
	queue_init_per_thread();


	if(tid == 0){
		numa_init();
		LPs_metada_init();
		pin_lps_on_numa_nodes_init();
		dymelor_init();
		statistics_init();
		queue_init();
		numerical_init();
		nodes_init();

		if(pdes_config.enforce_locality){
			pthread_barrier_init(&local_schedule_init_barrier, NULL, pdes_config.ncores);
			init_metrics_for_window();
  		}

  		if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT){
			state_swap_ptr = alloc_state_swapping_struct();
			pthread_create(&ipi_tid, NULL, signal_state_swapping, NULL);
		}

		nbcalqueue->hashtable->current  &= 0xffffffff;//MASK_EPOCH

	}

	//wait all threads to end the init phase to start togheter	
	// Set the CPU affinity
	__sync_fetch_and_add(&ready_wt, 1);
	while(ready_wt<pdes_config.ncores)usleep(500);

	while(lp_inizialized < pdes_config.nprocesses){
		//if(tid != 0) continue;
		current_lp = __sync_fetch_and_add(&lp_inizialized, 1);
		if(current_lp >=  pdes_config.nprocesses) continue;
		if(!pdes_config.serial) allocator_init_for_lp(current_lp);
		current_msg = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
 		current_msg->sender_id 		= -1;//
 		current_msg->receiver_id 	= current_lp;//
 		current_msg->timestamp 		= 0.0;//
 		current_msg->type 			= INIT;//
 		current_msg->state 			= 0x0;//
 		current_msg->father 		= NULL;//
 		current_msg->epoch 		= LPS[current_lp]->epoch;//
 		current_msg->monitor 		= 0x0;//
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound);
			ProcessEvent(current_lp, 0, INIT, NULL, 0, LPS[current_lp]->current_base_pointer); //current_lp = i;
			queue_deliver_msgs();
			LPS[current_lp]->bound = current_msg;
			LPS[current_lp]->num_executed_frames++;
			LPS[current_lp]->state_log_forced = true;
			LogState(current_lp);
			((state_t*)((rootsim_list*)LPS[current_lp]->queue_states)->head->data)->lvt = -1;// Required to exclude the INIT event from timeline
			LPS[current_lp]->state_log_forced = false;
			LPS[current_lp]->until_ongvt = 0;
			LPS[current_lp]->until_ckpt_recalc = 0;
	}

	__sync_fetch_and_add(&ready_wt, 1);
	while(ready_wt<pdes_config.ncores*2)usleep(500);


	if(pdes_config.enforce_locality) local_binding_init();

 	 set_affinity(tid);

	if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT){
		usleep(10);
		init_thread_csr_state();
	}
	
	if(tid == 0) {
		clock_timer_start(simulation_clocks);
    	timer_start(exec_time);
		printf("EXECUTED ALL INIT EVENTS\n");
	}


	if(tid == 0){
	    int ret;
		if( (ret = pthread_create(&sleeper, NULL, do_sleep, NULL)) != 0) {
	            fprintf(stderr, "%s\n", strerror(errno));
	            abort();
	    }
	}


    statistics_post_th_data(tid, STAT_INIT_CLOCKS, clock_timer_value(init_timer));
}

void executeEvent(unsigned int LP, simtime_t event_ts, unsigned int event_type, void * event_data, unsigned int event_data_size, void * lp_state, bool safety, msg_t * event){
//LP and event_ts are equal to current_LP and current_lvt, so they can be removed
//moreover, passing only the msg_t we could use only this one as input patameter
stat64_t execute_time;

#if REVERSIBLE == 1
	unsigned int mode;
#endif	
	queue_clean();
	
	(void)safety;
	(void)event;


#if REVERSIBLE == 1
	mode = 1;// ← ASK_EXECUTION_MODE_TO_MATH_MODEL(LP, evt.ts)
	if (safety || mode == 1){
#endif

#if REPORT == 1 
		clock_timer_start(event_processing_timer);
#endif


		ProcessEvent(LP, event_ts, event_type, event_data, event_data_size, lp_state);

#if REPORT == 1
		execute_time = clock_timer_value(event_processing_timer);

		statistics_post_lp_data(LP, STAT_EVENT, 1);
		statistics_post_lp_data(LP, STAT_CLOCK_EVENT, execute_time);
		statistics_post_th_data(tid, STAT_EVENT, 1);

		if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC){
			statistics_post_lp_data(LP, STAT_CLOCK_SILENT, execute_time);
			autockpt_update_ema_silent_event(LP, execute_time);
		}
		else
			autockpt_update_consecutive_forward_count(LP);
#endif
		/// computation of metrics for LP migration
		aggregate_metrics_for_lp_migration(current_lp, event->timestamp, LPS[current_lp]->bound->timestamp, execute_time);
		

#if VERBOSE == 1
		printf("Print LPS[current_lp]->ema_ti %f for current_lp %u\n", LPS[current_lp]->ema_ti, current_lp);
		printf("Print LPS[current_lp]->ema_granularity %f for current_lp %u\n", LPS[current_lp]->ema_granularity, current_lp);
		printf("migration_score %f for lp %u\n", LPS[current_lp]->migration_score, current_lp);
#endif


#if REVERSIBLE == 1
	}
	else {
		event->previous_seed = LPS[current_lp]->seed; //move outside the if_else
		event->revwin = revwin_create();
		revwin_reset(event->revwin);	//<-to do only one time at the beginnig of the execution 
		ProcessEvent_reverse(LP, event_ts, event_type, event_data, event_data_size, lp_state);
#if REPORT == 1 
		statistics_post_lp_data(LP, STAT_EVENT_REV, 1);
		statistics_post_lp_data(LP, STAT_CLOCK_REV, clock_timer_value(stm_event_processing_time));
		clock_timer_start(stm_safety_wait);
#endif
	}
#endif
		
	return;
}


void thread_loop(unsigned int thread_id) {
	unsigned int old_state;
	unsigned int empty_fetch = 0;
	
	
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif	
	init_simulation(thread_id);

	///* START SIMULATION *///
	while (  
		(
			 (pdes_config.timeout == 0 && !stop) || (pdes_config.timeout != 0 && !stop_timer)
		) 
		&& !sim_error
	) 
	{

		if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT){
	      #if CSR_CONTEXT == 0
			/// check the flag for sync output collection
			if (state_swap_ptr->state_swap_flag) {
			  #if VERBOSE == 1
				printf("committed state reconstruction operation\n");
			  #endif
				/// set global gvt variable
				update_global_gvt(local_gvt);
				csr_routine();
			}
	      #endif
			print_state_swapping_struct_metrics();
		}

		///* FETCH *///
	  #if REPORT == 1
		statistics_post_th_data(tid, STAT_EVENT_FETCHED, 1);
		clock_timer_start(fetch_timer);
	  #endif
	
		__event_from = 0;
		if(!am_i_committer()){
			if(pdes_config.enforce_locality && check_window() && local_fetch() != 0){} 
			else if(fetch_internal() == 0) {
			  #if REPORT == 1
				statistics_post_th_data(tid, STAT_EVENT_FETCHED_UNSUCC, 1);
				statistics_post_th_data(tid, STAT_CLOCK_FETCH_UNSUCC, (double)clock_timer_value(fetch_timer));
			  #endif

			  #if SWAPPING_STATE != 1
				if(pdes_config.ongvt_mode != MS_PERIODIC_ONGVT){
					if(++empty_fetch > 500){
						round_check_OnGVT();
					}
				}
			  #endif
			  	goto end_loop;
			}
		}
		else{
			fetch_internal();
			goto end_loop;
		}

		empty_fetch = 0;
		// Here we have the lock on the LP //
		// Locally (within the thread) copy lp and ts to processing event
		current_lp = current_msg->receiver_id;	// LP index
		current_lvt = current_msg->timestamp;	// Local Virtual Time
		current_evt_state   = current_msg->state;
		current_evt_monitor = current_msg->monitor;

	  #if DEBUG == 1
		assertf(current_evt_state == ELIMINATED, "got eliminatated message %p\n", current_msg);
		assertf(current_evt_state == NEW_EVT,    "got new_evt message %p\n", current_msg);
	  #endif

		if(__event_from == 2) statistics_post_th_data(tid, STAT_EVT_FROM_LOCAL_FETCH, 1);
		if(__event_from == 1) statistics_post_th_data(tid, STAT_EVT_FROM_GLOBAL_FETCH, 1);
		if(pdes_config.enforce_locality) local_binding_push(current_lp);
		
	  #if REPORT == 1
		statistics_post_lp_data(current_lp, STAT_EVENT_FETCHED_SUCC, 1);
		statistics_post_lp_data(current_lp, STAT_CLOCK_FETCH_SUCC, (double)clock_timer_value(fetch_timer));
	  #endif

	  #if DEBUG == 1
		if(!haveLock(current_lp)){//DEBUG
			printf(RED("[%u] Executing without lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
		}
		if(current_cpu != ((unsigned int)sched_getcpu())){
			printf("[%u] Current cpu changed form %u to %u\n", tid, current_cpu, sched_getcpu());
		}
	  #endif

	  #if DEBUG == 1	
		if(current_evt_state == ANTI_MSG && current_evt_monitor == (void*) 0xba4a4a) {
			printf(RED("TL: 0xba4a4a found!\n")); print_event(current_msg);
			gdb_abort;
			// FIXME: derived from the merge between stats and core
			unlock(current_lp);
			continue; //TODO: to verify
		}
	  #endif

		/* ROLLBACK */
		// Check whether the event is in the past, if this is the case
		// fire a rollback procedure.
		if(
			BEFORE_OR_CONCURRENT(current_msg, LPS[current_lp]->bound)
			//current_lvt < LPS[current_lp]->current_LP_lvt || 
			//(current_lvt == LPS[current_lp]->current_LP_lvt && current_msg->tie_breaker <= LPS[current_lp]->bound->tie_breaker)
			) {

	#if DEBUG == 1
			if(current_msg == LPS[current_lp]->bound && current_evt_state != ANTI_MSG) {
				if(current_evt_monitor == (void*) 0xba4a4a){
					printf(RED("There is a ba4a4a that is not an ANTI-MSG\n"));
				}
				printf(RED("Received multiple time the same event that is not an anti one\n!!!!!"));
				print_event(current_msg);
				gdb_abort;
			}
	#endif
			old_state = LPS[current_lp]->state;
			LPS[current_lp]->state = LP_STATE_ROLLBACK;

#if REPORT == 1 
			clock_timer_start(rollback_timer);
#endif

		#if DEBUG == 1
			LPS[current_lp]->last_rollback_event = current_msg;//DEBUG
			current_msg->roll_epoch = LPS[current_lp]->epoch;
			current_msg->rollback_time = CLOCK_READ();
		#endif
#if VERBOSE > 1
			printf("*** ROLLBACKING LP %u- da\n\t<%f,%u, %p> a \n\t<%f,%u,%p>\n", current_lp, 
				LPS[current_lp]->bound->timestamp, 	LPS[current_lp]->bound->tie_breaker,	LPS[current_lp]->bound , 
				current_msg->timestamp, 			current_msg->tie_breaker, 				current_msg);
#endif
			rollback(current_lp, current_lvt, current_msg->tie_breaker);
			statistics_post_th_data(tid, STAT_ROLLBACK, 1);

			LPS[current_lp]->state = old_state;

			//statistics_post_lp_data(current_lp, STAT_ROLLBACK, 1);
			if(current_evt_state != ANTI_MSG) {
				statistics_post_lp_data(current_lp, STAT_EVENT_STRAGGLER, 1);
			}
		}

		if(current_evt_state == ANTI_MSG) {
			current_msg->monitor = (void*) 0xba4a4a;

			statistics_post_lp_data(current_lp, STAT_EVENT_ANTI, 1);

			delete(nbcalqueue, current_node);
			unlock(current_lp);
			continue;
		}

	#if DEBUG==1
		if((unsigned long long)current_msg->node & (unsigned long long)0x1){
			printf(RED("Processing a deleted event"));
		}
	#endif

		// Update event and LP control variables
		current_msg->epoch = LPS[current_lp]->epoch;
	#if DEBUG == 1
		current_msg->execution_time = CLOCK_READ();
	#endif
		
		// The current_msg should be allocated with list allocator
		if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)) {
	#if DEBUG == 1 
			msg_t *bound_t, *next_t;
			bound_t = LPS[current_lp]->bound;
			next_t = list_next(LPS[current_lp]->bound);
	#endif
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound);
	#if DEBUG == 1
			if(BEFORE(current_msg, LPS[current_lp]->bound)){		printf("added event < bound after bound\n");    gdb_abort;}    
			if(list_next(current_msg) != next_t){					printf("list_next(current_msg) != next_t\n");	gdb_abort;}
			if(list_prev(current_msg) != bound_t){					printf("list_prev(current_msg) != bound_t\n");	gdb_abort;}
			if(list_next(bound_t) != current_msg){					printf("list_next(bound_t) != current_msg\n");	gdb_abort;}
			if(next_t!= NULL && list_prev(next_t) != current_msg){	printf("list_prev(next_t) != current_msg\n");	gdb_abort;}
			if(bound_t->timestamp > current_lvt){					printf("bound_t->timestamp > current_lvt\n");	gdb_abort;}			
			if(next_t!=NULL && next_t->timestamp < current_lvt){	printf("next_t->timestamp < current_lvt\n");	gdb_abort;}
	#endif
		}
		
		// Take a simulation state snapshot, in some way
		if(pdes_config.ncores > 1)
            LogState(current_lp);
		
#if DEBUG == 1
		if((unsigned long long)current_msg->node & 0x1){
			printf(RED("A - Node removed while processing it!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif
		///* PROCESS *///
		executeEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, LPS[current_lp]->current_base_pointer, safe, current_msg);

		///* FLUSH */// 
		queue_deliver_msgs();

#if DEBUG == 1
		if((unsigned long long)current_msg->node & 0x1){
			printf(RED("B - Node removed while processing it!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif

		// Update event and LP control variables
		LPS[current_lp]->bound = current_msg;
		current_msg->frame = LPS[current_lp]->num_executed_frames;
		LPS[current_lp]->num_executed_frames++;
		
		///* ON_GVT *///
		if(safe) {
			if(pdes_config.ongvt_mode == OPPORTUNISTIC_ONGVT && OnGVT(current_lp, LPS[current_lp]->current_base_pointer)){
				start_periodic_check_ongvt = 1;
				end_sim(current_lp);
			}
			LPS[current_lp]->until_ongvt = 0;
		}
		else if(pdes_config.ongvt_mode == EVT_PERIODIC_ONGVT && ((++(LPS[current_lp]->until_ongvt) % pdes_config.ongvt_period) == 0) ){
			check_OnGVT(current_lp, LPS[current_lp]->commit_horizon_ts, LPS[current_lp]->commit_horizon_tb);	
		}
    
		if((++(LPS[current_lp]->until_clean_ckp)%pdes_config.ckpt_collection_period  == 0)/* && safe*/){
            simtime_t gc_ts = LPS[current_lp]->commit_horizon_ts;
    
            if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT){
	            if(fossil_gvt > 0.0){
	         	   gc_ts = fossil_gvt;
	        	}
        	}

			clean_checkpoint(current_lp, gc_ts);
			clean_buffers_on_gvt(current_lp, gc_ts);
        }
    
	#if DEBUG == 0
		unlock(current_lp);
	#else				
		assertf(unlock(current_lp) == 0, "ERROR: unlock failed; previous value: %u\n", lp_lock[current_lp*CACHE_LINE_SIZE/4]);
	#endif

#if REPORT == 1
		clock_timer_start(queue_op);
#endif

		
		//COMMIT SAFE EVENT
		if(safe) {
			commit_event(current_msg, current_node, current_lp);
		}

//#if CSR_CONTEXT == 0
		//check if elapsed time since fetching an event is large enough to start computing stats
		aggregate_metrics_for_window_management(&w);

		/// do the unbalance check if the locality window is enabled or periodically
		if ( (w.enabled || periodic_rebalance_check(numa_rebalance_timer) ) ){
			//printf("Evaluating balance 1\n");
			if(enable_unbalance_check() ) { 
				if (pthread_mutex_trylock(&numa_balance_check_mtx) == 0) {
				  #if VERBOSE == 1
					printf("Evaluating balance 2\n");
				  #endif
					/// rebalancing algorithm and physical migration
                    if(num_numa_nodes > 1)
                        plan_and_do_lp_migration(numa_state);
					/// restart timer for next periodic rebalancing
					pthread_mutex_unlock(&numa_balance_check_mtx); 
				}
			}
		}
//#endif

		
end_loop:
		//CHECK END SIMULATION
		if(pdes_config.ongvt_period == EVT_PERIODIC_ONGVT){
			if(start_periodic_check_ongvt && pdes_config.ncores > 1){
				if((check_ongvt_period++ % pdes_config.ongvt_period) == 0){
					if(tryLock(last_checked_lp)){
						check_OnGVT(last_checked_lp, LPS[last_checked_lp]->commit_horizon_ts, LPS[last_checked_lp]->commit_horizon_tb);
						unlock(last_checked_lp);
						last_checked_lp = (last_checked_lp+1)%pdes_config.nprocesses;
					}
				}
			}
		}

		if(is_end_sim(current_lp)){
			if(check_termination()){
				__sync_bool_compare_and_swap(&stop, false, true);
			}
		}

		
		//LOCAL LISTS PRUNING
		nbc_prune();

		//PRINT REPORT
#if VERBOSE > 0
		if(tid == MAIN_PROCESS) {
			evt_count++;
			if (evt_count % 10000 == 0) {	
			//if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printf("TIME: %f - GVT: %f CQ_size: %llu CQ_count: %llu CQ_bw: %f\n", current_lvt, LPS[current_lp]->commit_horizon_ts, nbcalqueue->hashtable->size,nbcalqueue->hashtable->counter,nbcalqueue->hashtable->bucket_width);
				printf("Useless event in fetch: %f\n", diff_lp/(thread_stats[0].events_fetched));
				printf("Fetch length:           %f\n", thread_stats[0].events_get_next_fetch/(thread_stats[0].events_fetched));
				//printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
#endif
	}
#if REPORT == 1
	statistics_post_th_data(tid, STAT_CLOCK_LOOP, clock_timer_value(main_loop_time));
#endif


	if(pdes_config.enforce_locality)
		printf("Last window for %d is %f\n", tid, get_current_window());
	if(tid == 0 && pdes_config.ckpt_autonomic_period){
		unsigned int acc = 0;
		for(unsigned int i = 0; i<pdes_config.nprocesses;i++)
			acc += LPS[i]->ckpt_period;
		printf("AVG last ckpt period: %u\n", acc/pdes_config.nprocesses);
	}	
	
	// Unmount statistical data
	// FIXME
	//statistics_fini();
    
	if(sim_error){
		printf(RED("[%u] Execution ended for an error\n"), tid);
	} 
	else if (stop || stop_timer){
	    if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT)
		    while(!end_ipi);
    
		printf(GREEN( "[%u] Execution ended correctly\n"), tid);
		if(tid==0){
			if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT)
				pthread_join(ipi_tid, NULL);
			pthread_join(sleeper, NULL);
		}
      #if CSR_CONTEXT == 1
        destroy_thread_csr_state();
      #endif
	}
}
