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

#include <reverse.h>
#include <statistics.h>

#include "core.h"
#include "queue.h"
#include "nb_calqueue.h"
#include "simtypes.h"
#include "lookahead.h"
#include <hpdcs_utils.h>
#include <prints.h>


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
unsigned int check_ongvt_period = 0;

//__thread simtime_t 		commit_horizon_ts = 0;
//__thread unsigned int 	commit_horizon_tb = 0;


//timer
#if REPORT == 1
__thread clock_timer main_loop_time,		//OK: cattura il tempo totale di esecuzione sul singolo core...superflup
				queue_min_time,				//OK: cattura il tempo per fare un estrazione che va a buon fine 		
				event_processing_time,		//OK: cattura il tempo per processare un evento safe 
				queue_op,
				rollback_time
#if REVERSIBLE == 1
				,stm_event_processing_time
				,stm_safety_wait
#endif
				;
#endif

unsigned int ready_wt = 0;

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
bool stop = false;
unsigned long long *sim_ended;


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
		LPS[i]->ckpt_period 			= 10;
		LPS[i]->from_last_ckpt 			= 0;
		LPS[i]->state_log_forced  		= false;
		LPS[i]->current_base_pointer 	= NULL;
		LPS[i]->queue_in 				= new_list(i, msg_t);
		LPS[i]->bound 					= NULL;
		LPS[i]->queue_states 			= new_list(i, state_t);
		LPS[i]->mark 					= 0;
		LPS[i]->epoch 					= 1;
		LPS[i]->num_executed_frames		= 0;

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

// può diventare una macro?
// [D] no, non potrebbe più essere invocata lato modello altrimenti
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
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
	
	if(!is_end_sim(lp_idx)){
		// Ripristina stato sul commit_horizon
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
		// Ripristina stato sul bound
		LPS[lp_idx]->state = LP_STATE_ROLLBACK;
		//printf("%d- BUILD STATE AFTER GVT START LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		rollback(lp_idx, INFTY, UINT_MAX);
		//printf("%d- BUILD STATE AFTER GVT END LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		LPS[lp_idx]->state = old_state;
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
	
#if REVERSIBLE == 1
	reverse_init(REVWIN_SIZE);
#endif

	// Set the CPU affinity
	//set_affinity(tid);//TODO: decommentare per test veri
	
	// Initialize the set ??
	unsafe_set_init();


	to_remove_local_evts = new_list(tid, msg_t);
	freed_local_evts = new_list(tid, msg_t);
	
	if(tid == 0){
		LPs_metada_init();
		dymelor_init();
		statistics_init();
		queue_init();
		numerical_init();
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
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound); //ma qui il problema non era che non c'è il bound?
			ProcessEvent(current_lp, 0, INIT, NULL, 0, LPS[current_lp]->current_base_pointer); //current_lp = i;
			queue_deliver_msgs(); //Serve un clean della coda? Secondo me si! No, lo fa direttamente il metodo
			LPS[current_lp]->bound = current_msg;
			LPS[current_lp]->num_executed_frames++;
			LPS[current_lp]->state_log_forced = true;
			LogState(current_lp);
			LPS[current_lp]->state_log_forced = false;
			LPS[current_lp]->until_ongvt = 0;
			//LPS[current_lp]->epoch = 1;
		}
		printf("EXECUTED ALL INIT EVENTS\n");
	}


	//wait all threads to end the init phase to start togheter
	__sync_fetch_and_add(&ready_wt, 1);
	__sync_synchronize();
	while(ready_wt!=n_cores);
}

void executeEvent(unsigned int LP, simtime_t event_ts, unsigned int event_type, void * event_data, unsigned int event_data_size, void * lp_state, bool safety, msg_t * event){
//LP e event_ts sono gli stessi di current_LP e current_lvt, quindi potrebbero essere tolti
//inoltre, se passiamo solo il msg_t, possiamo evitare di passare gli altri parametri...era stato fatto così per mantenere il parallelismo con ProcessEvent
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
		clock_timer_start(event_processing_time);
#endif
		ProcessEvent(LP, event_ts, event_type, event_data, event_data_size, lp_state);		
#if REPORT == 1              
		if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
			statistics_post_data(tid, EVENTS_SAFE, 1);
			statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(event_processing_time));
		}else{
			statistics_post_data(tid, EVENTS_SAFE_SILENT, 1);
			statistics_post_data(tid, CLOCK_SAFE_SILENT, clock_timer_value(event_processing_time));
		}
#endif
#if REVERSIBLE == 1
	}
	else{
#if REPORT == 1 
		clock_timer_start(stm_event_processing_time);			
		statistics_post_data(tid, EVENTS_STM, 1);
#endif
		event->previous_seed = LPS[current_lp]->seed; //forse lo farei a prescindere mettendolo a fattor comune...son dati in cache
		event->revwin = revwin_create();
		revwin_reset(event->revwin);	//<-da mettere una volta sola ad inizio esecuzione
		ProcessEvent_reverse(LP, event_ts, event_type, event_data, event_data_size, lp_state);
#if REPORT == 1 
		statistics_post_data(tid, CLOCK_STM, clock_timer_value(stm_event_processing_time));
		clock_timer_start(stm_safety_wait);
#endif
	}
#endif
		
	return; //non serve tornare gli eventi prodotti, sono già visibili al thread
}


void thread_loop(unsigned int thread_id) {
	unsigned int old_state, empty_fetch = 0;
	
	init_simulation(thread_id);
	
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif	

	///* START SIMULATION *///
	while (!stop && !sim_error) {
		
		///* FETCH *///
#if REPORT == 1
		clock_timer_start(queue_min_time);
#endif
		if(fetch_internal() == 0) {
			if(++empty_fetch>500){
				round_check_OnGVT();
			}
			goto end_loop;
			continue;
		}
		empty_fetch = 0;

#if REPORT == 1
		statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_min_time));
		statistics_post_data(tid, EVENTS_FETCHED, 1);
#endif

		///* HOUSEKEEPING *///
		// Here we have the lock on the LP //

		// Locally (within the thread) copy lp and ts to processing event
		current_lp = current_msg->receiver_id;	// LP index
		current_lvt = current_msg->timestamp;	// Local Virtual Time
		current_evt_state   = current_msg->state;
		current_evt_monitor = current_msg->monitor;

	#if DEBUG == 1
		if(!haveLock(current_lp)){//DEBUG
			printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
		}
	#endif


	#if DEBUG == 1	
		if(current_evt_state == ANTI_MSG && current_evt_monitor == (void*) 0xba4a4a) {
			printf(RED("TL: ho trovato una banana!\n")); print_event(current_msg);
			gdb_abort;
			unlock(current_lp);
			continue; //TODO: verificare
		}
	#endif



		// Check whether the event is in the past, if this is the case
		// fire a rollback procedure.
		if(current_lvt < LPS[current_lp]->current_LP_lvt || 
			(current_lvt == LPS[current_lp]->current_LP_lvt && current_msg->tie_breaker <= LPS[current_lp]->bound->tie_breaker)
			) {
	#if DEBUG == 1
			if(current_msg == LPS[current_lp]->bound && current_evt_state != ANTI_MSG){
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
			clock_timer_start(rollback_time);
#endif

		#if DEBUG == 1
			LPS[current_lp]->last_rollback_event = current_msg;//DEBUG
			current_msg->roll_epoch = LPS[current_lp]->epoch;
			current_msg->rollback_time = CLOCK_READ();
		#endif
			//printf(YELLOW("ROLLBACK \n\tSTART: LID:%d LP.LVT:%f CURR_LVT:%f EX_FR:%d\n"), current_lp, LPS[current_lp]->current_LP_lvt, current_lvt, LPS[current_lp]->num_executed_frames);
			//printlp(YELLOW("\tStraggler received, I will do the LP rollback - Event [%.5f, %llu], LVT %.5f\n"), current_msg->timestamp, current_msg->tie_breaker, LPS[current_lp]->current_LP_lvt);
			rollback(current_lp, current_lvt, current_msg->tie_breaker);
			//printf(YELLOW("\tEND  : LID:%d LP.LVT:%f CURR_LVT:%f EX_FR:%d\n"), current_lp, LPS[current_lp]->current_LP_lvt, current_lvt, LPS[current_lp]->num_executed_frames);
			
#if REPORT == 1              
			statistics_post_data(tid, EVENTS_ROLL, 1);
			statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(rollback_time));
#endif
			LPS[current_lp]->state = old_state;
		}

		if(current_evt_state == ANTI_MSG) {
			current_msg->monitor = (void*) 0xba4a4a;
		#if DEBUG == 1
			assert(!list_is_connected(LPS[current_lp]->queue_in, current_msg));
		#endif
			delete(nbcalqueue, current_node);
			unlock(current_lp);
			continue;
		}

	#if DEBUG==1
		if((unsigned long long)current_msg->node & (unsigned long long)0x1){
			printf(RED("Si sta eseguendo un nodo eliminato"));
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
			if(list_next(current_msg) != next_t){					printf("list_next(current_msg) != next_t\n");	gdb_abort;}
			if(list_prev(current_msg) != bound_t){					printf("list_prev(current_msg) != bound_t\n");	gdb_abort;}
			if(list_next(bound_t) != current_msg){					printf("list_next(bound_t) != current_msg\n");	gdb_abort;}
			if(next_t!= NULL && list_prev(next_t) != current_msg){	printf("list_prev(next_t) != current_msg\n");	gdb_abort;}
			if(bound_t->timestamp > current_lvt){					printf("bound_t->timestamp > current_lvt\n");	gdb_abort;}			
			if(next_t!=NULL && next_t->timestamp < current_lvt){	printf("next_t->timestamp < current_lvt\n");	gdb_abort;}
	#endif
		}
				
		// Take a simulation state snapshot, in some way
		LogState(current_lp);
		
#if DEBUG == 1
		if((unsigned int)current_msg->node & 0x1){
			printf(RED("A - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
			print_event(current_msg);
			gdb_abort;				
		}
#endif
		
		
		///* PROCESS *///
		// Execute the event in the proper modality
		executeEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, LPS[current_lp]->current_base_pointer, safe, current_msg);

		///* FLUSH */// 
		queue_deliver_msgs();

#if DEBUG == 1
		if((unsigned int)current_msg->node & 0x1){
			printf(RED("B - Mi hanno cancellato il nodo mentre lo processavo!!!\n"));
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
		}else if((++(LPS[current_lp]->until_ongvt) % ONGVT_PERIOD) == 0){
			check_OnGVT(current_lp);	
		}
		
		
		if(safe) {
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


		
		if(safe) {
			commit_event(current_msg, current_node, current_lp);
			
			//prune_local_queue_with_ts(LPS[current_lp]->commit_horizon_ts);
			prune_local_queue_with_ts(local_gvt);
		}
		nbc_prune();
		//events_garbage_collection(commit_time);

#if REPORT == 1
		statistics_post_data(tid, CLOCK_PRUNE, clock_timer_value(queue_op));
		statistics_post_data(tid, PRUNE_COUNTER, 1);
#endif
		
end_loop:

		if(start_periodic_check_ongvt)
			if(check_ongvt_period++ % 100 == 0){
				if(tryLock(last_checked_lp)){
					check_OnGVT(last_checked_lp);
					unlock(last_checked_lp);
					last_checked_lp = (last_checked_lp+1)%n_prc_tot;
				}
			}

		if(is_end_sim(current_lp)){
			if(check_termination()){
				__sync_bool_compare_and_swap(&stop, false, true);
			}
		}
		
		if(tid == MAIN_PROCESS) {
			evt_count++;
			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printlp("TIME: %f", current_lvt);
				//printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
	}
#if REPORT == 1
	statistics_post_data(tid, CLOCK_LOOP, clock_timer_value(main_loop_time));
#endif


	// Unmount statistical data
	// FIXME
	//statistics_fini();
	
	if(sim_error){
		printf(RED("[%u] Execution ended for an error\n"), tid);
	} else if (stop){
		//sleep(5);
		printf(GREEN( "[%u] Execution ended correctly\n"), tid);
	}
}
