#define _GNU_SOURCE

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
//#include <string.h>
//#include <immintrin.h> //hardware transactional support
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <ROOT-Sim.h>
#include <dymelor.h>
#include <numerical.h>
#include <timer.h>
#include <state.h>

#include <reverse.h>
#include <statistics.h>

#include "core.h"
#include "queue.h"
#include "nb_calqueue.h"
#include "simtypes.h"
#include "lookahead.h"
#include <hpdcs_utils.h>
#include <prints.h>
#include <utmpx.h>


#include <numa_migration_support.h>
#include "metrics_for_lp_migration.h"

#if ENFORCE_LOCALITY == 1
#include "local_index/local_index.h"
#include "local_scheduler.h"
#include "metrics_for_window.h"
#include "clock_constant.h"
#endif
#include "metrics_for_window.h"

#if STATE_SWAPPING == 1
#include "state_swapping.h"
#endif

#define MAIN_PROCESS		0 //main process id
#define PRINT_REPORT_RATE	1000000000000000

#define MAX_PATHLEN			512


__thread simtime_t local_gvt = 0;

__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;
__thread unsigned long long current_evt_state = 0;
__thread void* current_evt_monitor = 0x0;


__thread struct __bucket_node *current_node = 0x0;

__thread unsigned int last_checked_lp = 0;
unsigned int start_periodic_check_ongvt = 0;
__thread unsigned int check_ongvt_period = 0;

__thread unsigned int current_numa_node;
__thread unsigned int current_cpu;


__thread int __event_from = 0;

//__thread simtime_t 		commit_horizon_ts = 0;
//__thread unsigned int 	commit_horizon_tb = 0;


//timer
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

simulation_configuration rootsim_config;

/* Total number of cores required for simulation */
unsigned int n_cores;
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot;

/* Commit horizon TODO */
 simtime_t gvt = 0;
/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1;

bool sim_error = false;

//void **states;
LP_state **LPS = NULL;

//used to check termination conditions
volatile bool stop = false;
volatile bool stop_timer = false;
pthread_t sleeper;//pthread_t p_tid[number_of_threads];//
unsigned int sec_stop = 0;
unsigned long long *sim_ended;

numa_struct **numa_state;			/// state of every numa node
pthread_mutex_t numa_balance_check_mtx; /// mutex for checking numa unbalance
unsigned int num_numa_nodes;
bool numa_available_bool;
unsigned int rounds_before_unbalance_check; /// number of window management rounds before numa unbalance check
clock_timer numa_rebalance_timer; /// timer to check for periodic numa rebalancing

#if STATE_SWAPPING == 1
  state_swapping_struct *state_swap_ptr; /// state swapping struct for output collection
#endif

size_t node_size_msg_t;
size_t node_size_state_t;

void * do_sleep(){
	if(sec_stop != 0)
		printf("The simulation will last %d seconds\n", sec_stop);
	sleep(sec_stop);
	if(sec_stop != 0)
		stop_timer = true;
	pthread_exit(NULL);
}



void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);
	fflush(stderr);

	if (fatal) {
		// Notify all KLT to shut down the simulation
		sim_error = true;
	}
}

/**
* This is an helper-function to allow the statistics subsystem create a new directory
*
* @author Alessandro Pellegrini
*
* @param path The path of the new directory to create
*/
void _mkdir(const char *path) {

	char opath[MAX_PATHLEN];
	char *p;
	size_t len;

	strncpy(opath, path, MAX_PATHLEN-1);
	len = strlen(opath);
	if (opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// Path plus 1 is a hack to allow also absolute path
	for (p = opath + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if (access(opath, F_OK)) {
		if (mkdir(opath, S_IRWXU)) {
			if (errno != EEXIST) {
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
			}
		}
	}
}

unsigned int get_current_numa_node(){
	if(numa_available() != -1){
		return numa_node_of_cpu(sched_getcpu());
	}
	return 1;
}


unsigned int cores_on_numa[N_CPU];

void set_affinity(unsigned int tid){
	unsigned int cpu_per_node;
	cpu_set_t mask;
	cpu_per_node = N_CPU/num_numa_nodes;
	
	
	current_cpu = ((tid % num_numa_nodes) * cpu_per_node + (tid/((unsigned int)num_numa_nodes)))%N_CPU;
	

	CPU_ZERO(&mask);
	CPU_SET(cores_on_numa[current_cpu], &mask);

	current_cpu = cores_on_numa[current_cpu];
	
	int err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}
	
	current_numa_node = get_current_numa_node();
	printf("Thread %2u set to CPU %2u on NUMA node %2u\n", tid, cores_on_numa[current_cpu], current_numa_node);
	
}

inline void SetState(void *ptr) { //make it as macro?
	ParallelSetState(ptr); 
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

void LPs_metada_init() {
	unsigned int i;
	int lp_lock_ret;

	//rootsim_config.output_dir = DEFAULT_OUTPUT_DIR;
	//rootsim_config.gvt_time_period = 1000;
	//rootsim_config.scheduler = SMALLEST_TIMESTAMP_FIRST;
	rootsim_config.checkpointing = PERIODIC_STATE_SAVING;
	rootsim_config.ckpt_period = 100;
	rootsim_config.gvt_snapshot_cycles = 2;
	rootsim_config.simulation_time = 0;
	//rootsim_config.lps_distribution = LP_DISTRIBUTION_BLOCK;
	//rootsim_config.check_termination_mode = NORM_CKTRM;
	rootsim_config.blocking_gvt = false;
	rootsim_config.snapshot = 2001;
	rootsim_config.deterministic_seed = false;
	rootsim_config.set_seed = 0;
	//rootsim_config.verbose = VERBOSE_INFO;
	//rootsim_config.stats = STATS_ALL;
	rootsim_config.serial = false;
	rootsim_config.core_binding = true;
	
	LPS = malloc(sizeof(void*) * n_prc_tot);
	sim_ended = malloc(LP_ULL_MASK_SIZE);
	lp_lock_ret =  posix_memalign((void **)&lp_lock, CACHE_LINE_SIZE, n_prc_tot * CACHE_LINE_SIZE); //  malloc(lps_num * CACHE_LINE_SIZE);
			
	if(LPS == NULL || sim_ended == NULL || lp_lock_ret == 1){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();
	}
	
	for (i = 0; i < n_prc_tot; i++) {
		lp_lock[i*(CACHE_LINE_SIZE/4)] = 0;
		sim_ended[i/64] = 0;
		LPS[i] = malloc(sizeof(LP_state));
		LPS[i]->lid 					= i;
		LPS[i]->seed 					= i+1; //TODO;
		LPS[i]->state 					= LP_STATE_READY;
		LPS[i]->ckpt_period 			= CHECKPOINT_PERIOD;
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
	  #if ENFORCE_LOCALITY == 1
		bzero(&LPS[i]->local_index, sizeof(LPS[i]->local_index));
		LPS[i]->wt_binding = UNDEFINED_WT;
	  #endif

		/* FIELDS FOR COMPUTING AVG METRICS	*/
		LPS[i]->ema_ti					= 0.0;
		LPS[i]->ema_granularity			= 0.0;
		LPS[i]->migration_score			= 0.0;
		
	}
	
	for(; i<(LP_BIT_MASK_SIZE) ; i++)
		end_sim(i);

	
}

void numa_init(){
	//TODO: make it as macro if possible
	if(numa_available() != -1){
		printf("NUMA machine with %u nodes.\n", (unsigned int)  numa_num_configured_nodes());
		numa_available_bool = true;
		num_numa_nodes = numa_num_configured_nodes();
	}
	else{
		printf("UMA machine.\n");
		numa_available_bool = false;
		num_numa_nodes = 1;
	}	



	numa_state = malloc(num_numa_nodes * sizeof(numa_struct *));

	for (unsigned int i=0; i < num_numa_nodes; i++) {
		numa_state[i] = malloc(sizeof(numa_struct));
		alloc_numa_state(numa_state[i]);
		numa_state[i]->numa_id = i;
	}


	
	/// bitmask for each numa node
	struct bitmask *numa_mask[num_numa_nodes];

	if (num_numa_nodes > 1) {

	    for (int i=0; i < num_numa_nodes; i++) {
	            numa_mask[i] = numa_allocate_cpumask();
	            numa_node_to_cpus(i, numa_mask[i]);
	    }
	}

	/// set a global array in which are set the cpus on the same numa node
	/// indexed from 0 to cpu_per_node and from cpu_per_node to N_CPU-1
	int insert_idx = 0;
	for (int j=0; j < num_numa_nodes; j++) {
		for (int k=0; k < N_CPU; k++) {
			if (num_numa_nodes ==1 || numa_bitmask_isbitset(numa_mask[j], k)) {
				cores_on_numa[insert_idx] = k; 
				insert_idx++;
			}
		}
	}



}


void pin_lps_on_numa_nodes_init() {

	unsigned cur_lp = 0;
	for(unsigned int j=0;j<num_numa_nodes;j++){
		for(unsigned int i=0;i<n_prc_tot/num_numa_nodes;i++) {
			set_bit(numa_state[j]->numa_binding_bitmap, cur_lp);
			LPS[cur_lp]->numa_node 				= j;
		  #if VERBOSE == 1
			printf("INI NUMA %d LP %d b:%d\n",  j, cur_lp, get_bit(numa_state[j]->numa_binding_bitmap, cur_lp));
		  #endif
			cur_lp++;
		}
	}
	for(;cur_lp<n_prc_tot;cur_lp++){
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

// can be replaced with a macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
}


//#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event),event, event->node);


void check_OnGVT(unsigned int lp_idx){
	unsigned int old_state;
	current_lp = lp_idx;
	LPS[lp_idx]->until_ongvt = 0;
	
	if(!is_end_sim(lp_idx)){
		//Restore state on the commit_horizon
		old_state = LPS[lp_idx]->state;
		LPS[lp_idx]->state = LP_STATE_ONGVT;
		//printf("%d- BUILD STATE FOR %d TIMES GVT START LVT:%f commit_horizon:%f\n", current_lp, LPS[current_lp]->until_ongvt, LPS[current_lp]->current_LP_lvt, commit_horizon_ts);
		rollback(lp_idx, LPS[lp_idx]->commit_horizon_ts, LPS[lp_idx]->commit_horizon_tb);
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
		rollback(lp_idx, INFTY, UINT_MAX);
		//printf("%d- BUILD STATE AFTER GVT END LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		LPS[lp_idx]->state = old_state;

		statistics_post_lp_data(lp_idx, STAT_ONGVT, 1);
	}
}

/// worker threads for csr routine
//int worker_threads;

void csr_routine(){

	__sync_fetch_and_add(&state_swap_ptr->worker_threads, 1);
	state_swap_ptr->counter_lp = n_prc_tot-1;

	int cur_lp = state_swap_ptr->counter_lp;

	state_swapping_struct *new_state_swap_ptr;

	int potential_locked_object;

	bool to_release = true; //TODO: to be used in new trylock implementation

	while(cur_lp >= 0) {

	/// actual state output collection
	  #if VERBOSE == 1
		printf("csr_routine lp %u\n", cur_lp);
	  #endif
		//TODO: change tryLock semantics
		if (tryLock(cur_lp)) {
			if (!get_bit(state_swap_ptr->lp_bitmap, cur_lp)) {
				
				if(LPS[cur_lp]->commit_horizon_ts>0){

					//swap_state(cur_lp, state_swap_ptr->reference_gvt);
					OnGVT(cur_lp, LPS[cur_lp]->current_base_pointer);
					//restore_state(cur_lp);

					set_bit(state_swap_ptr->lp_bitmap, cur_lp);
				}

			} ///end if get_bit

			if (to_release) unlock(cur_lp);
		} ///end if trylock

		cur_lp = __sync_fetch_and_add(&state_swap_ptr->counter_lp, -1);

	}///end while loop

	/*if (locks[potential_locked_object]) = MakeWord(1, 1, tid) {
		if (!get_bit(state_swap_ptr->lp_bitmap, potential_locked_object)) {
			swap_state(potential_locked_object, state_swap_ptr->reference_gvt);
			OnGVT(potential_locked_object, state_pointers[potential_locked_object]);
			restore_state(potential_locked_object);
			set_bit(state_swap_ptr->lp_bitmap, potential_locked_object);
		}
	}*/

	__sync_fetch_and_add(&state_swap_ptr->worker_threads, -1);
	if (state_swap_ptr->worker_threads == 1) {
		/// allocation of a new state_swapping ptr and cas must be done one time per-round
		//new_state_swap_ptr = alloc_state_swapping_struct();
		//__sync_bool_compare_and_swap(&state_swap_ptr, state_swap_ptr, new_state_swap_ptr);
		reset_state_swapping_struct(&state_swap_ptr);
	  #if VERBOSE == 1
		print_state_swapping_struct(state_swap_ptr);
	  #endif
	}

	/// syscall to restore normal context
	//restore_context();
	
}

void round_check_OnGVT(){
	unsigned int start_from = (n_prc_tot / n_cores) * tid;
	unsigned int curent_ck = start_from;
	
	
	//printf("OnGVT: la coda è vuota!\n");
	
	do{
		if(!is_end_sim(curent_ck)){
			if(tryLock(curent_ck)){
				//commit_horizon_ts = 5000;
				//commit_horizon_tb = 100;
				if(LPS[curent_ck]->commit_horizon_ts>0){
//					printf("[%u]ROUND ONGVT LP:%u TS:%f TB:%llu\n", tid, curent_ck,LPS[curent_ck]->commit_horizon_ts, LPS[curent_ck]->commit_horizon_tb);
	
					check_OnGVT(curent_ck);
				}
				unlock(curent_ck);
				break;
			}
		}
		curent_ck = (curent_ck + 1) % n_prc_tot;
	}while(curent_ck != start_from);
}

void init_simulation(unsigned int thread_id){
	tid = thread_id;
	int i = 0;
	
#if REVERSIBLE == 1
	reverse_init(REVWIN_SIZE);
#endif

	
	// Initialize the set ??
	unsafe_set_init();

	for(;i<THR_POOL_SIZE;i++)
	{
		_thr_pool.messages[i].father = 0;
	}
	_thr_pool._thr_pool_count = 0;

	to_remove_local_evts = new_list(tid, msg_t);
	to_remove_local_evts_old = new_list(tid, msg_t);
	freed_local_evts = new_list(tid, msg_t);



	
	if(tid == 0){
		numa_init();
		LPs_metada_init();
		pin_lps_on_numa_nodes_init();
		dymelor_init(n_prc_tot);
		statistics_init();
		queue_init();
		numerical_init();
		nodes_init();
		//process_init_event
		for (current_lp = 0; current_lp < n_prc_tot; current_lp++) {	
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
				LPS[current_lp]->ckpt_period = 20;
				//LPS[current_lp]->epoch = 1;
		}

              #if ENFORCE_LOCALITY==1
		pthread_barrier_init(&local_schedule_init_barrier, NULL, n_cores);
		init_metrics_for_window();
              #endif
		#if STATE_SWAPPING == 1
		  state_swap_ptr = alloc_state_swapping_struct();
		#endif
		nbcalqueue->hashtable->current  &= 0xffffffff;//MASK_EPOCH
		printf("EXECUTED ALL INIT EVENTS\n");
	}


	//wait all threads to end the init phase to start togheter

	if(tid == 0 && do_sleep != 0){
	    int ret;
		if( (ret = pthread_create(&sleeper, NULL, do_sleep, NULL)) != 0) {
	            fprintf(stderr, "%s\n", strerror(errno));
	            abort();
	    }
	}
	
	__sync_fetch_and_add(&ready_wt, 1);
	__sync_synchronize();
	while(ready_wt!=n_cores);
	// Set the CPU affinity
	set_affinity(tid);
  #if ENFORCE_LOCALITY == 1
	local_binding_init();
  #endif
	
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

		if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC){
			statistics_post_lp_data(LP, STAT_CLOCK_SILENT, execute_time);
		}
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
	
#if ONGVT_PERIOD != -1
	unsigned int empty_fetch = 0;
#endif
	
	init_simulation(thread_id);
	
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif	

	///* START SIMULATION *///
	while (  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {

#if STATE_SWAPPING == 1

		//TODO: compute reference_gvt

		/// check the flag for sync output collection
		if (state_swap_ptr->state_swap_flag) {
		  #if VERBOSE == 1
			printf("committed state reconstruction operation\n");
		  #endif
			csr_routine();
		}

		/// periodically set the flag to 1
		if (!state_swap_ptr->state_swap_flag) signal_state_swapping();
	

#endif

		///* FETCH *///
#if REPORT == 1
		statistics_post_th_data(tid, STAT_EVENT_FETCHED, 1);
		clock_timer_start(fetch_timer);
#endif
	__event_from = 0;
#if ENFORCE_LOCALITY == 1

		if(check_window() && local_fetch() != 0){ //if window_size > 0 try local_fetch

		} else  
#endif
		
		if(fetch_internal() == 0) {
#if REPORT == 1
			statistics_post_th_data(tid, STAT_EVENT_FETCHED_UNSUCC, 1);
			statistics_post_th_data(tid, STAT_CLOCK_FETCH_UNSUCC, (double)clock_timer_value(fetch_timer));
#endif

#if ONGVT_PERIOD != -1
			if(++empty_fetch > 500){
				round_check_OnGVT();
			}
#endif
			goto end_loop;
		}

#if ONGVT_PERIOD != -1
		empty_fetch = 0;
#endif

		///* HOUSEKEEPING *///
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
  #if ENFORCE_LOCALITY == 1
		local_binding_push(current_lp);
  #endif
		
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
			
			LPS[current_lp]->state = old_state;

			statistics_post_lp_data(current_lp, STAT_ROLLBACK, 1);
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
		
#if CKPT_RECALC == 1
		// Check whether to recalculate the checkpoint interval
		if (LPS[current_lp]->until_ckpt_recalc++ % CKPT_RECALC_PERIOD == 0) {
			checkpoint_interval_recalculate(current_lp);
		}
#endif

		// Take a simulation state snapshot, in some way
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
			if(OnGVT(current_lp, LPS[current_lp]->current_base_pointer)){
				start_periodic_check_ongvt = 1;
				end_sim(current_lp);
			}
			LPS[current_lp]->until_ongvt = 0;
		}
#if ONGVT_PERIOD != -1
		else if((++(LPS[current_lp]->until_ongvt) % ONGVT_PERIOD) == 0){
			check_OnGVT(current_lp);	
		}
#endif	
		
		if((++(LPS[current_lp]->until_clean_ckp)%CLEAN_CKP_INTERVAL  == 0)/* && safe*/){
			clean_checkpoint(current_lp, LPS[current_lp]->commit_horizon_ts);
			//ATTENTION: We should take care of the tie_breaker?
			clean_buffers_on_gvt(current_lp, LPS[current_lp]->commit_horizon_ts);
		}
		
	#if DEBUG == 0
		unlock(current_lp);
	#else				
		if(!unlock(current_lp)) {
			printlp("ERROR: unlock failed; previous value: %u\n", lp_lock[current_lp]);
		}
	#endif

#if REPORT == 1
		clock_timer_start(queue_op);
#endif

		
		//COMMIT SAFE EVENT
		if(safe) {
			commit_event(current_msg, current_node, current_lp);
		}


//#if ENFORCE_LOCALITY == 1		
		//check if elapsed time since fetching an event is large enough to start computing stats
		aggregate_metrics_for_window_management(&w);
//#endif
		/// do the unbalance check if the locality window is enabled or periodically
		if ( (w.enabled || periodic_rebalance_check(numa_rebalance_timer) ) ){
			//printf("Evaluating balance 1\n");
			if(enable_unbalance_check() ) { 
				if (pthread_mutex_trylock(&numa_balance_check_mtx) == 0) {
				  #if VERBOSE == 0
					printf("Evaluating balance 2\n");
				  #endif
					/// rebalancing algorithm and physical migration
					plan_and_do_lp_migration(numa_state);
					/// restart timer for next periodic rebalancing
					pthread_mutex_unlock(&numa_balance_check_mtx); 
				}
			}
		}

#if REPORT == 1
		//statistics_post_th_data(tid, STAT_CLOCK_PRUNE, clock_timer_value(queue_op));
		//statistics_post_th_data(tid, STAT_PRUNE_COUNTER, 1);
#endif
		
end_loop:
		//CHECK END SIMULATION
#if ONGVT_PERIOD != -1
		if(start_periodic_check_ongvt){
			if(check_ongvt_period++ % ONGVT_PERIOD == 0){
				if(tryLock(last_checked_lp)){
					check_OnGVT(last_checked_lp);
					unlock(last_checked_lp);
					last_checked_lp = (last_checked_lp+1)%n_prc_tot;
				}
			}
		}
#endif

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


#if ENFORCE_LOCALITY == 1
	printf("Last window for %d is %f\n", tid, get_current_window());
#endif
	// Unmount statistical data
	// FIXME
	//statistics_fini();
	
	if(sim_error){
		printf(RED("[%u] Execution ended for an error\n"), tid);
	} else if (stop || stop_timer){
		//sleep(5);
		printf(GREEN( "[%u] Execution ended correctly\n"), tid);
		if(tid==0){
			pthread_join(sleeper, NULL);

		}
	}
}
