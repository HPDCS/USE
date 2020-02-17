#define _GNU_SOURCE

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
//#include <immintrin.h> //supporto transazionale
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

#if PREEMPT_COUNTER==1
#include <preempt_counter.h>
#endif

#if POSTING==1
#include <posting.h>
#endif

#if DEBUG==1
#include <checks.h>
#endif

#if LONG_JMP==1
#include <jmp.h>
#endif

#if IPI_SUPPORT==1
#include <ipi.h>
#include <lp_stats.h>
#endif

#if HANDLE_INTERRUPT==1
#include <handle_interrupt.h>
#include <lp_stats.h>
#endif

#if HANDLE_INTERRUPT_WITH_CHECK==1
#include <handle_interrupt_with_check.h>
#endif

//id del processo principale
#define MAIN_PROCESS		0
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

//__thread simtime_t 		commit_horizon_ts = 0;
//__thread unsigned int 	commit_horizon_tb = 0;

//timer
#if REPORT == 1
__thread clock_timer main_loop_time,		//OK: cattura il tempo totale di esecuzione sul singolo core...superflup
				fetch_timer,				//OK: cattura il tempo per fare un estrazione che va a buon fine 		
				event_processing_timer,		//OK: cattura il tempo per processare un evento safe 
				queue_op,
				rollback_timer
#if REVERSIBLE == 1
				,stm_event_processing_time
				,stm_safety_wait
#endif

#if HANDLE_INTERRUPT==1	
				,event_handler_timer
#endif
				;
#endif

volatile unsigned int ready_wt = 0;
// volatile unsigned int ended_wt = 0;

simulation_configuration rootsim_config;

/* Total number of cores required for simulation */
unsigned int n_cores; //pls cambia nome
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot; //pls cambia nome


/* Commit horizon TODO */
 simtime_t gvt = 0;
/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1; //Non saprei che metterci

bool sim_error = false;

//void **states;
LP_state **LPS = NULL;

//used to check termination conditions
volatile bool stop = false;
volatile bool stop_timer = false;
pthread_t sleeper;//pthread_t p_tid[number_of_threads];//
unsigned int sec_stop = 0;
unsigned long long *sim_ended;

size_t node_size_msg_t;
size_t node_size_state_t;

#if LONG_JMP==1
__thread cntx_buf cntx_loop;
#endif

void * do_sleep(){
	printf("The process will last %d seconds\n", sec_stop);
	
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

	strncpy(opath, path, sizeof(opath));
	len = strlen(opath);
	if (opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// opath plus 1 is a hack to allow also absolute path
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


void set_affinity(unsigned int tid){
	unsigned int id_cpu;
	cpu_set_t mask;	
	id_cpu = (tid % 8) * 4 + (tid/((unsigned int)8));
	printf("Thread %u set to CPU no %u\n", tid, id_cpu);
	CPU_ZERO(&mask);
	CPU_SET(id_cpu, &mask);
	int err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}
}

inline void SetState(void *ptr) { //può diventare una macro?
	ParallelSetState(ptr); 
}

void nodes_init(){
		size_t tmp_size;
		unsigned int i;
		
		i = 0;
		tmp_size = sizeof(struct rootsim_list_node) + sizeof(msg_t);//multiplo di 64
		while((++i)*CACHE_LINE_SIZE < tmp_size);
		node_size_msg_t = (i)*CACHE_LINE_SIZE;
		
		i = 0;
		tmp_size = sizeof(struct rootsim_list_node) + sizeof(state_t);//multiplo di 64
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
#if CONSTANT_CHILD_INVALIDATION==1
		set_epoch_of_LP(i,1);
#else
		LPS[i]->epoch 					= 1;
#endif
		LPS[i]->num_executed_frames		= 0;
		LPS[i]->until_clean_ckp			= 0;

#if HANDLE_INTERRUPT==1
		LPS[i]->dummy_bound=NULL;
		LPS[i]->LP_state_is_valid=true;//start with LP state valid
		LPS[i]->old_valid_bound=NULL;
#endif
#if POSTING==1
		LPS[i]->priority_message=NULL;
#endif
#if IPI_SUPPORT==1
		LPS[i]->lp_statistics = NULL;//LP event's statistic initialization, struct will be allocated by LP-0 at init time
#endif
	}
	
	for(; i<(LP_BIT_MASK_SIZE) ; i++)
		end_sim(i);

	
}

//Sostituirlo con una bitmap!
//Nota: ho visto che viene invocato solo a fine esecuzione
bool check_termination(void) {
	unsigned int i;
	
	
	for (i = 0; i < LP_MASK_SIZE; i++) {
		if(sim_ended[i] != ~(0ULL))
			return false; 
	}
	
	return true;
}

void insert_ordered_in_list(unsigned int lid,struct rootsim_list_node* queue_in,msg_t*starting_event,msg_t*event_to_insert){
	if(event_to_insert==NULL)
		return;//don't insert NULL messagges
	if(!list_is_connected(queue_in, event_to_insert)) {
		msg_t *bound_t, *next_t,*prev_t;
		bound_t = starting_event;
		prev_t=bound_t;
		next_t = list_next(prev_t);//first event after bound
		while (next_t!=NULL){
		#if DEBUG==1
			if( (prev_t->timestamp > next_t->timestamp)
				|| ( (prev_t->timestamp==next_t->timestamp) && (prev_t->tie_breaker>next_t->tie_breaker)) ){
				printf("event not in order in local_queue\n");
				gdb_abort;
			}
		#endif
			if( (next_t->timestamp> event_to_insert->timestamp)
				|| (next_t->timestamp==event_to_insert->timestamp && next_t->tie_breaker> event_to_insert->tie_breaker) ){
				break;
			}
			else{
				prev_t=next_t;
				next_t=list_next(next_t);
			}
		}
		list_place_after_given_node_by_content(lid, queue_in, event_to_insert,prev_t);
		#if DEBUG == 1
		if(list_next(event_to_insert) != next_t){					printf("list_next(current_msg) != next_t\n");	gdb_abort;}
		if(list_prev(event_to_insert) != prev_t){					printf("list_prev(current_msg) != prev_t\n");	gdb_abort;}
		if(list_next(prev_t) != event_to_insert){					printf("list_next(prev_t) != current_msg\n");	gdb_abort;}
		if(next_t!= NULL && list_prev(next_t) != event_to_insert){	printf("list_prev(next_t) != current_msg\n");	gdb_abort;}
		if(next_t!=NULL && next_t->timestamp < event_to_insert->timestamp){	printf("next_t->timestamp < current_lvt\n");	gdb_abort;}
		if(prev_t !=NULL && prev_t->timestamp > event_to_insert->timestamp){	printf("prev_t->timestamp > current_lvt\n");	gdb_abort;}			
		if(next_t!=NULL && next_t->timestamp < event_to_insert->timestamp){	printf("next_t->timestamp < current_lvt\n");	gdb_abort;}
		#endif
	}
	return;
}
void insert_current_msg_after_given_event(msg_t*given_event){
	// The current_msg should be allocated with list allocator
	if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)) {
		//insert_msg after old_valid_bound

#if DEBUG==1//not present in original version
		if(current_msg->frame!=0){
			printf("event not connected to localqueue,but it was executed!!!\n");
			gdb_abort;
		}
#endif
		#if DEBUG == 1 
		msg_t *bound_t, *next_t;
		bound_t = given_event;
		next_t = list_next(bound_t);
		#endif
		list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, given_event);
		#if DEBUG == 1
		if(list_next(current_msg) != next_t){					printf("list_next(current_msg) != next_t\n");	gdb_abort;}
		if(list_prev(current_msg) != bound_t){					printf("list_prev(current_msg) != bound_t\n");	gdb_abort;}
		if(list_next(bound_t) != current_msg){					printf("list_next(bound_t) != current_msg\n");	gdb_abort;}
		if(next_t!= NULL && list_prev(next_t) != current_msg){	printf("list_prev(next_t) != current_msg\n");	gdb_abort;}
		if(bound_t->timestamp > current_lvt){					printf("bound_t->timestamp > current_lvt b\n");	gdb_abort;}			
		if(next_t!=NULL && next_t->timestamp < current_lvt){	printf("next_t->timestamp < current_lvt\n");	gdb_abort;}
		#endif
	}
}
void deallocate_event(msg_t*event){
	list_node_clean_by_content(event); 
	list_insert_tail_by_content(freed_local_evts, event);
	return;
}

msg_t *allocate_event(unsigned int lp_idx){
	msg_t*event=list_allocate_node_buffer_from_list(lp_idx, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
	list_node_clean_by_content(event);
	return event;
}

// può diventare una macro?
// [D] no, non potrebbe più essere invocata lato modello altrimenti
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
    	#if DEBUG==1//not present in original version
		check_ScheduleNewEventFuture();
    	#endif
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
	#if DEBUG==1
	else{
		check_ScheduleNewEventPast();
	}
	#endif
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
///////////			 _	 _	 _	 _	      _   __      __		/////////////////////////////////////
///////////			| \	| |	| \	/ \      / \ |  | /| |  |		/////////////////////////////////////
///////////			|_/	|_|	| | '-.  __   _/ |  |  |  ><		/////////////////////////////////////
///////////			|	| |	|_/	\_/      /__ |__|  | |__|		/////////////////////////////////////
///////////														/////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////



//#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event),event, event->node);


void check_OnGVT(unsigned int lp_idx){
	unsigned int old_state;
	current_lp = lp_idx;
	LPS[lp_idx]->until_ongvt = 0;
	
	#if HANDLE_INTERRUPT==1
	unsigned int old_current_lp=current_lp;
	msg_t*old_current_msg=current_msg;
	#endif
	if(!is_end_sim(lp_idx)){
		// Ripristina stato sul commit_horizon
		old_state = LPS[lp_idx]->state;
		#if DEBUG==1
		if(old_state!=LP_STATE_READY){
			printf("old state is not LP_STATE_READY in check_OnGVT function\n");
			gdb_abort;
		}
		#endif
		LPS[lp_idx]->state = LP_STATE_ONGVT;
		//printf("%d- BUILD STATE FOR %d TIMES GVT START LVT:%f commit_horizon:%f\n", current_lp, LPS[current_lp]->until_ongvt, LPS[current_lp]->current_LP_lvt, commit_horizon_ts);
		#if HANDLE_INTERRUPT==1
		//overwrite current_lp and current_msg,because this function is called in round_robin manner,function nested starting from these can take decision reading current_lp and current_msg!
		current_lp=lp_idx;
		current_msg=NULL;
		LPS[current_lp]->old_valid_bound=LPS[current_lp]->bound;
		#endif

		#if HANDLE_INTERRUPT_WITH_CHECK==1 //make this code not interruptible
		enter_in_unpreemptable_zone();
		#endif
		rollback(lp_idx, LPS[lp_idx]->commit_horizon_ts, LPS[lp_idx]->commit_horizon_tb);
		//printf("%d- BUILD STATE FOR %d TIMES GVT END\n", current_lp );
		#if HANDLE_INTERRUPT_WITH_CHECK==1
		exit_from_unpreemptable_zone();
		#endif
		//printf("[%u]ONGVT LP:%u TS:%f TB:%llu\n", tid, lp_idx,LPS[lp_idx]->commit_horizon_ts, LPS[lp_idx]->commit_horizon_tb);
		if(OnGVT(lp_idx, LPS[lp_idx]->current_base_pointer)){
			//printf("Simulation endend on LP:%u\n", lp_idx);
			start_periodic_check_ongvt = 1;
			end_sim(lp_idx);
		}
		// Ripristina stato sul bound
		LPS[lp_idx]->state = LP_STATE_ROLLBACK;

		//printf("%d- BUILD STATE AFTER GVT START LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		
		//if rollback won't be interrupted then restore current_lp and current_msg and LP_state_is_valid=true
		//else control flow go to pre fetch_internal so current_lp,current_msg will be reinitialized
		rollback(lp_idx, INFTY, UINT_MAX);
		//printf("%d- BUILD STATE AFTER GVT END LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		LPS[lp_idx]->state = old_state;
		#if HANDLE_INTERRUPT==1
		LPS[current_lp]->LP_state_is_valid=true;//LP state is been restorered by rollback
		LPS[current_lp]->dummy_bound->state=NEW_EVT;
		//restore bound,current_lp and current_msg
		LPS[current_lp]->bound=LPS[current_lp]->old_valid_bound;
		LPS[current_lp]->old_valid_bound=NULL;
		current_lp=old_current_lp;
		current_msg=old_current_msg;
		#endif
		statistics_post_lp_data(lp_idx, STAT_ONGVT, 1);
	}
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

	// Set the CPU affinity
	//set_affinity(tid);//TODO: decommentare per test veri
	
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

#if IPI_SUPPORT==1
	//register thread in order to send and to receive IPI
	register_thread_to_ipi_module(tid,"cfv_trampoline",(unsigned long)cfv_trampoline);
#endif

#if PREEMPT_COUNTER==1
	initialize_preempt_counter(tid);//init counter
#endif

#if POSTING==1
    //initialize collision_list
    for (int i=0; i<MAX_THR_HASH_TABLE_SIZE; i++)
    {
        _thr_pool.collision_list[i]=NULL;
    }
#endif
	if(tid == 0){
		LPs_metada_init();
		printf("LP_metada init finished\n");
#if IPI_SUPPORT==1
		init_lp_stats(LPS, n_prc_tot);//allocation of lp_stats structs for all LPs
		printf("LP_stats init finished\n");
#endif
		dymelor_init();
		printf("Dymelor_init finished\n");
		statistics_init();
		printf("Statistics_init finished\n");
		queue_init();
		printf("Queue_init finished\n");
		numerical_init();
		printf("Numerical_init finished\n");
		nodes_init();
		printf("Nodes_init finished\n");
		#if IPI_SUPPORT==1
		run_time_data_init();////register helpful data available only at run-time,main thread do run_time_data_init
		#endif
		//process_init_event
		#if IPI_SUPPORT==1 && DEBUG==1
		current_lp = 0;//make current_lp valid value,it will be reset to 0 in the next for loop
		check_trampoline_function();
		printf("all checks passed on trampoline function\n");
		#endif
		for (current_lp = 0; current_lp < n_prc_tot; current_lp++) {
       		current_msg = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), (struct rootsim_list*) freed_local_evts);
       		current_msg->sender_id 		= -1;//
       		current_msg->receiver_id 	= current_lp;//
       		current_msg->timestamp 		= 0.0;//
       		current_msg->type 			= INIT;//
       		current_msg->state 			= NEW_EVT;//
       		current_msg->father 		= NULL;//
#if CONSTANT_CHILD_INVALIDATION==1
       		current_msg->epoch= get_epoch_of_LP(current_lp);
#else
       		current_msg->epoch 		= LPS[current_lp]->epoch;//
#endif
       		current_msg->monitor 		= 0x0;//
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound); //ma qui il problema non era che non c'è il bound?
#if HANDLE_INTERRUPT==1
			current_msg->evt_start_time = 0ULL;//event INIT does not require to be monitored
#endif
			ProcessEvent(current_lp, 0, INIT, NULL, 0, LPS[current_lp]->current_base_pointer); //current_lp = i;
			queue_deliver_msgs(); //Serve un clean della coda? Secondo me si! No, lo fa direttamente il metodo
			LPS[current_lp]->bound = current_msg;
			LPS[current_lp]->num_executed_frames++;
			LPS[current_lp]->state_log_forced = true;
			LogState(current_lp);
			((state_t*)((rootsim_list*)LPS[current_lp]->queue_states)->head->data)->lvt = -1;// Serve per tirare fuori l'INIT dalla timeline
			LPS[current_lp]->state_log_forced = false;
			LPS[current_lp]->until_ongvt = 0;
			LPS[current_lp]->until_ckpt_recalc = 0;
			//LPS[current_lp]->ckpt_period = 20;
			//LPS[current_lp]->epoch = 1;
#if HANDLE_INTERRUPT==1
			//after execution of INIT_EVENTS alloc dummy_bound
			LPS[current_lp]->dummy_bound=allocate_dummy_bound(current_lp);
			LPS[current_lp]->msg_curr_executed=NULL;
#endif
		}
		nbcalqueue->hashtable->current  &= 0xffffffff;//MASK_EPOCH
		printf("EXECUTED ALL INIT EVENTS\n");

	}

	//wait all threads to end the init phase to start togheter

	if(tid == 0)
	{
	    int ret;
		if( (ret = pthread_create(&sleeper, NULL, do_sleep, NULL)) != 0) {
	            fprintf(stderr, "%s\n", strerror(errno));
	            abort();
	    }
	}

	__sync_fetch_and_add(&ready_wt, 1);
	__sync_synchronize();
	while(ready_wt!=n_cores);
}

void executeEvent(unsigned int LP, simtime_t event_ts, unsigned int event_type, void * event_data, unsigned int event_data_size, void * lp_state, bool safety, msg_t * event){
//LP e event_ts sono gli stessi di current_LP e current_lvt, quindi potrebbero essere tolti
//inoltre, se passiamo solo il msg_t, possiamo evitare di passare gli altri parametri...era stato fatto così per mantenere il parallelismo con ProcessEvent
stat64_t execute_time;

#if REVERSIBLE == 1
	unsigned int mode;
#endif	
	queue_clean();//che succede se lascio le parentesi ad una macro?
	
	(void)safety;
	(void)event;

	//IF evt.LP.localNext != NULL //RELATIVO AL FRAME
	// marcare in qualche modo evt.LP.localNext…non sono sicurissimo sia da marcare come da eliminare…se è da rieseguire che si fa? Collegato ai frame

#if REVERSIBLE == 1
	mode = 1;// ← ASK_EXECUTION_MODE_TO_MATH_MODEL(LP, evt.ts)
	if (safety || mode == 1){
#endif

#if REPORT == 1 
		clock_timer_start(event_processing_timer);
#endif
#if DEBUG==1//not present in original version
		if(LPS[LP]->state!=LP_STATE_READY && LPS[LP]->state!=LP_STATE_SILENT_EXEC){
			printf("invalid event state,state is %d\n",LPS[LP]->state);
			gdb_abort;
		}
#endif

		#if HANDLE_INTERRUPT_WITH_CHECK==1
		if(LPS[current_lp]->state==LP_STATE_SILENT_EXEC){
			enter_in_preemptable_zone();
		}//in forward mode preemptability is already actived in thread_loop

		#if DEBUG==1
		check_preemptability();
		#endif

		#endif
		
		ProcessEvent(LP, event_ts, event_type, event_data, event_data_size, lp_state);
		
		#if HANDLE_INTERRUPT_WITH_CHECK==1
		if(LPS[current_lp]->state==LP_STATE_SILENT_EXEC){
			exit_from_preemptable_zone();
		}//in forward mode preemptability remains actived,it ends after queu_deliver_msgs
		#endif

#if REPORT == 1
		execute_time = clock_timer_value(event_processing_timer);

		statistics_post_lp_data(LP, STAT_EVENT, 1);
		statistics_post_lp_data(LP, STAT_CLOCK_EVENT, execute_time);

		if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC){
			statistics_post_lp_data(LP, STAT_CLOCK_SILENT, execute_time);
		}
		else{
			statistics_post_lp_data(LP, STAT_CLOCK_FORWARD, execute_time);
		}
#endif

#if REVERSIBLE == 1
	}
	else {
		event->previous_seed = LPS[current_lp]->seed; //forse lo farei a prescindere mettendolo a fattor comune...son dati in cache
		event->revwin = revwin_create();
		revwin_reset(event->revwin);	//<-da mettere una volta sola ad inizio esecuzione
		ProcessEvent_reverse(LP, event_ts, event_type, event_data, event_data_size, lp_state);
#if REPORT == 1 
		statistics_post_lp_data(LP, STAT_EVENT_REV, 1);
		statistics_post_lp_data(LP, STAT_CLOCK_REV, clock_timer_value(stm_event_processing_time));
		clock_timer_start(stm_safety_wait);
#endif
	}
#endif
		
	return; //non serve tornare gli eventi prodotti, sono già visibili al thread
}

#if HANDLE_INTERRUPT==1
void thread_loop(unsigned int thread_id) {
	unsigned int old_state=0;
#if ONGVT_PERIOD != -1
	unsigned int empty_fetch = 0;
#endif

	//init with section not interruptible!!!
	init_simulation(thread_id);

#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif

	int res=set_jmp(&cntx_loop);

	#if HANDLE_INTERRUPT_WITH_CHECK==1 && DEBUG==1
	check_unpreemptability();
	#endif
	//this switch is not interruptible in every case!!!
	switch (res){
		case CFV_INIT :
			#if DEBUG==1
			check_CFV_INIT();
			#endif

			#if VERBOSE > 0
			printf("[IPI_4_USE] - First time we saved the execution context!!!\n");
			#endif

			break;//CFV_INIT
		#if IPI_SUPPORT==1
		case CFV_TO_HANDLE :
			 //We will return to this point in the
			 //execution upon a longjmp invocation
			 //with same "jmp_buf" data.
			//USE THIS BRANCH TO RESET ANY GLOBAL
			//AND LOCAL VARIABLE THAT MAY RESULT
			//INCONSISTENT AFTER "longjmp" CALL. 	
		#if DEBUG==1
			check_CFV_TO_HANDLE();
		#endif
			
		#if VERBOSE>0
			printf("[IPI_4_USE] - Thread %d jumped back from an interrupted execution!!!\n",tid);
		#endif

		#if REPORT==1
        	statistics_post_th_data(tid,STAT_IPI_RECEIVED,1);
        #endif
        	if(LPS[current_lp]->state==LP_STATE_READY){
        		statistics_post_lp_data(current_lp,STAT_EVENT_FORWARD_INTERRUPTED,1);
        		statistics_post_lp_data(current_lp,STAT_CLOCK_EXEC_EVT_INTER_FORWARD_EXEC,clock_timer_value(event_handler_timer));
        	}
        	else{
        		statistics_post_lp_data(current_lp,STAT_EVENT_SILENT_INTERRUPTED,1);
        		statistics_post_lp_data(current_lp,STAT_CLOCK_EXEC_EVT_INTER_SILENT_EXEC,clock_timer_value(event_handler_timer));
        	}
            if(current_msg==NULL){//event interrupted in silent execution with IPI,but there is no current_msg
            	#if DEBUG==1
            	check_CFV_TO_HANDLE_current_msg_null();
            	#endif
            	make_LP_state_invalid(LPS[current_lp]->old_valid_bound);
            }
			else if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC){
				#if DEBUG==1
				check_CFV_TO_HANDLE_past();
				#endif//DEBUG
				insert_ordered_in_list(current_lp,(struct rootsim_list_node*)LPS[current_lp]->queue_in,LPS[current_lp]->last_silent_exec_evt,current_msg);
				make_LP_state_invalid(list_prev(current_msg));
			}
			else{
				//event interrupt in future
				#if DEBUG==1
				check_CFV_TO_HANDLE_future();
				#endif
				make_LP_state_invalid(LPS[current_lp]->old_valid_bound);
			}
			#if DEBUG==1
			reset_nesting_counters();
			#endif
			unlock(current_lp);
			break;
		#endif
		case CFV_ALREADY_HANDLED ://nop to do
		#if VERBOSE > 0
			printf("cfv already handled,tid=%d\n",tid);
		#endif
		#if POSTING==1 || IPI_SUPPORT==1 && REPORT==1
			statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_USEFUL,1);
		#endif
			#if DEBUG==1
			check_CFV_ALREADY_HANDLED();
			#endif
			unlock(current_lp);
			break;
		default ://error
			printf("invalid return value in set_jmp,long_jmp called with value %d\n",res);
			gdb_abort;
			break;
	}

	#if DEBUG==1
	check_thread_loop_before_fetch();
	#endif

	//after this line code may became interruptible!!
	// START SIMULATION //
	while (  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {
		
		// FETCH //
#if REPORT == 1
		//statistics_post_th_data(tid, STAT_EVENT_FETCHED, 1);
		clock_timer_start(fetch_timer);
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
		// HOUSEKEEPING //
		// Here we have the lock on the LP //
		// Locally (within the thread) copy lp and ts to processing event
		current_lp = current_msg->receiver_id;	// LP index
		current_lvt = current_msg->timestamp;	// Local Virtual Time
		current_evt_state   = current_msg->state;
		current_evt_monitor = current_msg->monitor;

		#if DEBUG==1//not present in orginal version
		check_thread_loop_after_fetch();
		#endif
		
#if REPORT == 1
		statistics_post_lp_data(current_lp, STAT_EVENT_FETCHED_SUCC, 1);
		statistics_post_lp_data(current_lp, STAT_CLOCK_FETCH_SUCC, (double)clock_timer_value(fetch_timer));
#endif
		change_bound_with_current_msg();
		//now bound is current_msg-epsilon,current_msg appares in future in respect of other threads
		//now bound is corrupted,remember to restore it before next fetch!!

		if ((current_lvt < LPS[current_lp]->old_valid_bound->timestamp || 
			(current_lvt == LPS[current_lp]->old_valid_bound->timestamp && current_msg->tie_breaker <= LPS[current_lp]->old_valid_bound->tie_breaker)
			)
			|| LPS[current_lp]->LP_state_is_valid==false)
			{
			//LP state is invalid or current_msg in past: restore LP state before execute current_msg
			#if DEBUG == 1
			if(current_msg == LPS[current_lp]->old_valid_bound && current_evt_state != ANTI_MSG) {
				if(current_evt_monitor == (void*) 0xba4a4a){
					printf(RED("Ho una banana che non è ANTI-MSG\n"));
				}
				printf(RED("Ho ricevuto due volte di fila lo stesso messaggio e non è un antievento\n!!!!!"));
				print_event(current_msg);
				gdb_abort;
			}
			#endif

			old_state = LPS[current_lp]->state;
			LPS[current_lp]->state = LP_STATE_ROLLBACK;

		#if DEBUG == 1
			LPS[current_lp]->last_rollback_event = current_msg;//DEBUG
			#if CONSTANT_CHILD_INVALIDATION==1
			current_msg->roll_epoch= get_epoch_of_LP(current_lp);
			#else
			current_msg->roll_epoch = LPS[current_lp]->epoch;
			#endif
			current_msg->rollback_time = CLOCK_READ();
		#endif
#if VERBOSE > 0
			printf("*** Sto ROLLBACKANDO LP %u- da\n\t<%f,%u, %p> a \n\t<%f,%u,%p>\n", current_lp, 
				LPS[current_lp]->old_valid_bound->timestamp, 	LPS[current_lp]->old_valid_bound->tie_breaker,	LPS[current_lp]->old_valid_bound , 
				current_msg->timestamp,current_msg->tie_breaker,current_msg);
#endif		
			rollback(current_lp, current_lvt, current_msg->tie_breaker);
			
			LPS[current_lp]->state = old_state;
			statistics_post_lp_data(current_lp, STAT_ROLLBACK, 1);
			if(LPS[current_lp]->LP_state_is_valid==true){//if event was in past before rollback
				if(current_evt_state != ANTI_MSG) {
					statistics_post_lp_data(current_lp, STAT_EVENT_STRAGGLER, 1);
				}
				else{
					statistics_post_lp_data(current_lp, STAT_EVENT_ANTI, 1);
				}
			}
			LPS[current_lp]->LP_state_is_valid=true;//LP state is been restorered by rollback
			if(LPS[current_lp]->dummy_bound->state==ROLLBACK_ONLY){
				LPS[current_lp]->dummy_bound->state=NEW_EVT;
				LPS[current_lp]->bound=LPS[current_lp]->old_valid_bound;
				LPS[current_lp]->old_valid_bound=NULL;
				unlock(current_lp);//release lock
				continue;//this event cannot be processed,rollback was used only to restore state,so give up this event and fetch another event
			}
		}

		//now LP state is valid current_msg in future respect of dummy_bound and old_valid_bound,but bound corrupted
		#if DEBUG==1//not present in original version
			check_after_rollback();
		#endif//DEBUG
		//now LP state is valid and event is in future
		if(current_evt_state == ANTI_MSG) {
			current_msg->monitor = (void*) 0xba4a4a;
			current_msg->frame= tid+1;//not present in original version
			delete(nbcalqueue, current_node);
			LPS[current_lp]->bound=LPS[current_lp]->old_valid_bound;
			LPS[current_lp]->old_valid_bound=NULL;
			unlock(current_lp);//release lock
			continue;//
		}
		
	#if DEBUG==1
		if((unsigned long long)current_msg->node & (unsigned long long)0x1){
			printf(RED("Si sta eseguendo un nodo eliminato"));
		}
	#endif
		
		#if CONSTANT_CHILD_INVALIDATION==1
		current_msg->epoch = get_epoch_of_LP(current_lp);
		#else
		// Update event and LP control variables
		current_msg->epoch = LPS[current_lp]->epoch;
		#endif
		insert_current_msg_after_given_event(LPS[current_lp]->old_valid_bound);

#if CKPT_RECALC == 1
		// Check whether to recalculate the checkpoint interval
		if (LPS[current_lp]->until_ckpt_recalc++ % CKPT_RECALC_PERIOD == 0) {
			checkpoint_interval_recalculate(current_lp);
		}
#endif
		
#if DEBUG == 1
		if((unsigned long long)current_msg->node & 0x1){
			printf(RED("A - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif
		#if HANDLE_INTERRUPT==1
		start_exposition_of_current_event(current_msg);
		#endif

		#if HANDLE_INTERRUPT_WITH_CHECK==1
		enter_in_preemptable_zone();
		#endif
		// PROCESS //
		executeEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, LPS[current_lp]->current_base_pointer, safe, current_msg);
		// FLUSH // 
		
		queue_deliver_msgs();

		#if HANDLE_INTERRUPT_WITH_CHECK==1
		exit_from_preemptable_zone();
		#endif

		#if HANDLE_INTERRUPT==1
		end_exposition_of_current_event(current_msg);
		#endif

#if DEBUG==1//not present in original version
	    check_thread_loop_after_executeEvent();
#endif

		// Update event and LP control variables
		LPS[current_lp]->bound = current_msg;
		current_msg->frame = LPS[current_lp]->num_executed_frames;
		LPS[current_lp]->num_executed_frames++;
		LPS[current_lp]->old_valid_bound = NULL;//reset old_valid_bound

		// Take a simulation state snapshot, in some way
		LogState(current_lp);

		// ON_GVT //
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
		
		if(safe && (++(LPS[current_lp]->until_clean_ckp)%CLEAN_CKP_INTERVAL  == 0) ){
				clean_checkpoint(current_lp, LPS[current_lp]->commit_horizon_ts);
		}

	#if DEBUG == 0
		unlock(current_lp);
	#else				
		if(!unlock(current_lp)) {
			printlp("ERROR: unlock failed; previous value: %u\n", lp_lock[current_lp]);
		}
	#endif

		//COMMIT SAFE EVENT
		if(safe) {
			#if REPORT==1
			statistics_post_lp_data(current_lp,STAT_EVENTS_EXEC_AND_COMMITED,1);
			#endif
			commit_event(current_msg, current_node, current_lp);
		}

#if REPORT == 1
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
		if(tid == MAIN_PROCESS) {
			evt_count++;
			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printlp("TIME: %f", current_lvt);
				//printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
	}
#if REPORT == 1
	statistics_post_th_data(tid, STAT_CLOCK_LOOP, clock_timer_value(main_loop_time));
#endif

#if IPI_SUPPORT==1
	if(tid==0)
		ipi_print_and_reset_counters();
	ipi_unregister();
#endif

	if(sim_error){
		printf(RED("[%u] Execution ended for an error\n"), tid);
	} 
	else if (stop || stop_timer){
		//sleep(5);
		printf(GREEN( "[%u] Execution ended correctly\n"), tid);
		if(tid==0){
			pthread_join(sleeper, NULL);
		}
	}
}

#else//HANDLE_INTERRUPT
void thread_loop(unsigned int thread_id) {
	unsigned int old_state=0;
#if ONGVT_PERIOD != -1
	unsigned int empty_fetch = 0;
#endif
#if PREEMPT_COUNTER==1
	//init counter
	initialize_preempt_counter(thread_id);
#endif
	init_simulation(thread_id);
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif

#if LONG_JMP==1
	int res=set_jmp(&cntx_loop);
	switch (res){
		case CFV_INIT :
			#if DEBUG==1
			if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
				printf("preempt counter is not INIT in CFV_INIT\n");
				gdb_abort;
			}
			#endif
			printf("[IPI_4_USE] - First time we saved the execution context!!!\n");
			break;//CFV_INIT
		case CFV_ALREADY_HANDLED ://nop to do
			#if DEBUG==1
			if(*preempt_count_ptr!=PREEMPT_COUNT_INIT){
				printf("preempt counter is not INIT in CFV_ALREADY_HANDLED\n");
				gdb_abort;
			}

			#endif
			#if VERBOSE > 0
			printf("cfv already handled,tid=%d\n",tid);
			#endif
#if POSTING==1 && REPORT==1
			statistics_post_lp_data(current_lp,STAT_SYNC_CHECK_USEFUL,1);
#endif
			unlock(current_lp);
			break;
		default ://error
			printf("invalid return value in set_jmp,long_jmp called with value %d\n",res);
			gdb_abort;
			break;
	}
#endif//#if LONG_JMP

	#if DEBUG==1
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
	#endif


	///* START SIMULATION *///
	while (  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {
		
		///* FETCH *///
#if REPORT == 1
		//statistics_post_th_data(tid, STAT_EVENT_FETCHED, 1);
		clock_timer_start(fetch_timer);
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
		#if DEBUG==1//not present in orginal version
		if (current_msg->tie_breaker==0){
			printf("fetched event with tie_breaker zero\n");
			print_event(current_msg);
			gdb_abort;
		}
		if(LPS[current_lp]->state != LP_STATE_READY){
			printf("LP state is not ready\n");
			gdb_abort;
		}
		#endif

#if DEBUG==1//not present in original version
		if( (current_evt_state!=ANTI_MSG) && (current_evt_state!= EXTRACTED) ){
			printf("current_evt_state=%lld\n",current_evt_state);
			gdb_abort;
		}
#endif
		
#if REPORT == 1
		statistics_post_lp_data(current_lp, STAT_EVENT_FETCHED_SUCC, 1);
		statistics_post_lp_data(current_lp, STAT_CLOCK_FETCH_SUCC, (double)clock_timer_value(fetch_timer));
#endif



	#if DEBUG == 1
		if(!haveLock(current_lp)){//DEBUG
			printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
		}
	#endif

	#if DEBUG == 1	
		if(current_evt_state == ANTI_MSG && current_evt_monitor == (void*) 0xba4a4a) {
			printf(RED("TL: ho trovato una banana!\n")); print_event(current_msg);
			gdb_abort;
			// FIXME: deriva dal merge tra stats e core
			unlock(current_lp);
			continue; //TODO: verificare
		}
	#endif
		/* ROLLBACK */
		// Check whether the event is in the past, if this is the case
		// fire a rollback procedure.
		if(current_lvt < LPS[current_lp]->current_LP_lvt || 
			(current_lvt == LPS[current_lp]->current_LP_lvt && current_msg->tie_breaker <= LPS[current_lp]->bound->tie_breaker)
			) {

	#if DEBUG == 1
			if(current_msg == LPS[current_lp]->bound && current_evt_state != ANTI_MSG) {
				if(current_evt_monitor == (void*) 0xba4a4a){
					printf(RED("Ho una banana che non è ANTI-MSG\n"));
				}
				printf(RED("Ho ricevuto due volte di fila lo stesso messaggio e non è un antievento\n!!!!!"));
				print_event(current_msg);
				//gdb_abort;
			}
	#endif

			old_state = LPS[current_lp]->state;
			LPS[current_lp]->state = LP_STATE_ROLLBACK;

#if REPORT == 1 
			clock_timer_start(rollback_timer);
#endif

		#if DEBUG == 1
			LPS[current_lp]->last_rollback_event = current_msg;//DEBUG
			#if CONSTANT_CHILD_INVALIDATION==1
			current_msg->roll_epoch= get_epoch_of_LP(current_lp);
			#else
			current_msg->roll_epoch = LPS[current_lp]->epoch;
			#endif
			current_msg->rollback_time = CLOCK_READ();
		#endif
#if VERBOSE > 0
			printf("*** Sto ROLLBACKANDO LP %u- da\n\t<%f,%u, %p> a \n\t<%f,%u,%p>\n", current_lp, 
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
		#if DEBUG==1//not present in original version
			if(current_msg->timestamp<LPS[current_lp]->bound->timestamp
			|| ((current_msg->timestamp==LPS[current_lp]->bound->timestamp) && (current_msg->tie_breaker<=LPS[current_lp]->bound->tie_breaker)) ){
				printf("event in past\n");
				gdb_abort;
			}
		#endif//DEBUG
		//now LP state is valid and event is in future
		if(current_evt_state == ANTI_MSG) {

			current_msg->monitor = (void*) 0xba4a4a;
			current_msg->frame= tid+1;//not present in original version
			statistics_post_lp_data(current_lp, STAT_EVENT_ANTI, 1);
			delete(nbcalqueue, current_node);
			unlock(current_lp);
			continue;
		}
		
	#if DEBUG==1
		if((unsigned long long)current_msg->node & (unsigned long long)0x1){
			printf(RED("Si sta eseguendo un nodo eliminato"));
		}
	#endif

		#if CONSTANT_CHILD_INVALIDATION==1
		current_msg->epoch = get_epoch_of_LP(current_lp);
		#else
		// Update event and LP control variables
		current_msg->epoch = LPS[current_lp]->epoch;
		#endif
	#if DEBUG == 1
		current_msg->execution_time = CLOCK_READ();
	#endif
		// The current_msg should be allocated with list allocator
		if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)) {

#if DEBUG==1//not present in original version
			if(current_msg->frame!=0){
				printf("event not connected to localqueue,but it was executed!!!\n");
				gdb_abort;
			}
#endif

	#if DEBUG == 1 
			msg_t *bound_t, *next_t;
			bound_t = LPS[current_lp]->bound;
			next_t = list_next(LPS[current_lp]->bound);
	#endif
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound);
	#if DEBUG == 1
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
			printf(RED("A - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif
		///* PROCESS *///
		executeEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, LPS[current_lp]->current_base_pointer, safe, current_msg);
		///* FLUSH */// 
		#if PREEMPT_COUNTER==1
		#if INTERRUPT_FORWARD==1
		#else
		// increment_preempt_counter();
		#endif
		#endif
		queue_deliver_msgs();
		#if PREEMPT_COUNTER==1
		#if INTERRUPT_FORWARD==1
		#else
		// decrement_preempt_counter();
		#endif
		#endif
#if DEBUG==1//not present in original version
    if(_thr_pool._thr_pool_count!=0){//thread pool count must be 0 after queue_deliver_msgs
        printf("not empty pool count,tid=%d\n",tid);
        gdb_abort;
    } 
#endif

#if POSTING==1 && DEBUG==1
    for(unsigned int i=0;i<MAX_THR_HASH_TABLE_SIZE;i++){
        if(_thr_pool.collision_list[i]!=NULL){
            printf("not empty collision list,tid=%d\n",tid);
            gdb_abort;
        }

    }
#endif

#if DEBUG == 1
		if((unsigned long long)current_msg->node & 0x1){
			printf(RED("B - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif
		// Update event and LP control variables
		#if DEBUG==1//not present in original version
		if(LPS[current_lp]->bound->frame != LPS[current_lp]->num_executed_frames-1){
			printf("invalid frame number in event,frame_event=%d,frame_LP=%d\n",LPS[current_lp]->bound->frame,LPS[current_lp]->num_executed_frames);
			gdb_abort;
		}
		if(LPS[current_lp]->bound->epoch > current_msg->epoch){
			printf("invalid epoch number in event,epoch_bound=%d,epoch_current_msg=%d\n",LPS[current_lp]->bound->epoch,current_msg->epoch);
			gdb_abort;
		}
		#endif
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
		
		if(safe && (++(LPS[current_lp]->until_clean_ckp)%CLEAN_CKP_INTERVAL  == 0) ){
				clean_checkpoint(current_lp, LPS[current_lp]->commit_horizon_ts);
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
		if(tid == MAIN_PROCESS) {
			evt_count++;
			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printlp("TIME: %f", current_lvt);
				//printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
	}
#if REPORT == 1
	statistics_post_th_data(tid, STAT_CLOCK_LOOP, clock_timer_value(main_loop_time));
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
#endif
