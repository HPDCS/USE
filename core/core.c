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
#include "hpdcs_utils.h"


//id del processo principale
#define MAIN_PROCESS		0
#define PRINT_REPORT_RATE	1000000000000000

#define MAX_PATHLEN			512


__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;

//timer
#if REPORT == 1
__thread clock_timer main_loop_time,		//OK: cattura il tempo totale di esecuzione sul singolo core...superflup
				queue_min_time,				//OK: cattura il tempo per fare un estrazione che va a buon fine 		
				event_processing_time,		//OK: cattura il tempo per processare un evento safe 
				queue_op;
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
	rootsim_config.ckpt_period = 10;
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
		LPS[i]->seed 					= i; //TODO;
		LPS[i]->state 					= LP_STATE_READY;
		LPS[i]->ckpt_period 			= 10;
		LPS[i]->from_last_ckpt 			= 0;
		LPS[i]->state_log_forced  		= false;
		LPS[i]->current_base_pointer 	= NULL;
		LPS[i]->queue_in 				= new_list(i, msg_t);
		LPS[i]->bound 					= NULL;
		//LPS[i]->queue_out 				= new_list(i, msg_hdr_t);
		LPS[i]->queue_states 			= new_list(i, state_t);
		LPS[i]->mark 					= 0;
		LPS[i]->epoch 					= 0;
		LPS[i]->num_executed_frames		= 0;

	}
	
	for(; i<(LP_BIT_MASK_SIZE) ; i++)
		end_sim(i);

	
}

//Sostituirlo con una bitmap!
//Nota: ho visto che viene invocato solo a fine esecuzione
bool check_termination(void) {
	unsigned int i;
	
	
	//for (i = 0; i < n_prc_tot; i++) {
	//	if(!is_end_sim(i)) //[i] != ~(0ULL))
	//		return false; 
	//}
	
	//for (i = 0; i < n_prc_tot/64; i++) {
	//	if(sim_ended[i] != ~(0ULL))
	//		return false; 
	//}
	
	for (i = 0; i < LP_MASK_SIZE; i++) {
		if(sim_ended[i] != ~(0ULL))
			return false; 
	}
	
	return true;
}

//può diventare una macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC){
		queue_insert(receiver, timestamp, event_type, event_content, event_size);
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////			 _	 _	 _	 _	      _   __      __		//////////////////////////////////////////////////////////////////////////////////////
///////////			| \	| |	| \	/ \      / \ |  | /| |  |		//////////////////////////////////////////////////////////////////////////////////////
///////////			|_/	|_|	| | '-.  __   _/ |  |  |  ><		//////////////////////////////////////////////////////////////////////////////////////
///////////			|	| |	|_/	\_/      /__ |__|  | |__|		//////////////////////////////////////////////////////////////////////////////////////
///////////														//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void init_simulation(unsigned int thread_id){
	tid = thread_id;
	
#if REVERSIBLE == 1
	reverse_init(REVWIN_SIZE);
#endif

	//Set the CPU affinity
	set_affinity(tid);
	
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
       		current_msg = list_allocate_node_buffer_from_list(current_lp, sizeof(msg_t), freed_local_evts);
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound); //ma qui il problema non era che non c'è il bound?
			ProcessEvent(current_lp, 0, INIT, NULL, 0, LPS[current_lp]->current_base_pointer); //current_lp = i;
			queue_deliver_msgs(); //Serve un clean della coda? Secondo me si! No, lo fa direttamente il metodo
			LPS[current_lp]->bound = current_msg;
			LPS[current_lp]->num_executed_frames++;
			LPS[current_lp]->state_log_forced = true;
			LogState(current_lp);
			LPS[current_lp]->state_log_forced = false;
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
	queue_clean;//che succede se lascio le parentesi ad una macro?
	
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
		statistics_post_data(tid, EVENTS_SAFE, 1);
		statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(event_processing_time));
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
	unsigned int old_state;
	
	init_simulation(thread_id);
	
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif	

	///* START SIMULATION *///
	while (!stop && !sim_error) {
		
		/// *FETCH* ///
#if REPORT == 1
		clock_timer_start(queue_min_time);
#endif
		if(fetch_internal() == 0){
			continue;
		}
#if REPORT == 1
		statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_min_time));
		statistics_post_data(tid, EVENTS_FETCHED, 1);
#endif

		//locally copy lp and ts to processing event
		current_lp = current_msg->receiver_id;	//identificatore lp
		current_lvt = current_msg->timestamp;	//local virtual time

		if(current_lvt < LPS[current_lp]->current_LP_lvt || 
			(current_lvt == LPS[current_lp]->current_LP_lvt && current_msg->tie_breaker < LPS[current_lp]->bound->tie_breaker)
			){
			old_state = LPS[current_lp]->state;
			LPS[current_lp]->state = LP_STATE_ROLLBACK;
			rollback(current_lp, current_lvt, current_msg->tie_breaker);
			LPS[current_lp]->state = old_state;//
		}
		if(current_msg->state ==ANTI_EVENT){
			unlock(current_lp);
			continue; //TODO: verificare
		}


		//update event and LP control variables
		current_msg->epoch = LPS[current_lp]->epoch;
		

		//Inserisco l'evento nella coda locale, se non c'è già. Si può spostare dopo l'esecuzione per correttezza strutturale
		// The current_msg should be allocated with list allocator
		if(!list_is_connected(LPS[current_lp]->queue_in, current_msg)){
			list_place_after_given_node_by_content(current_lp, LPS[current_lp]->queue_in, current_msg, LPS[current_lp]->bound);
		}
				
		//save in some way the state of the simulation
		LogState(current_lp);
		
		/// *PROCESS* ///
		//execute the event in the proper modality
		executeEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, LPS[current_lp]->current_base_pointer, safe, current_msg);

		///* FLUSH */// 
		queue_deliver_msgs();

		//update event and LP control variables
		LPS[current_lp]->bound = current_msg;
		LPS[current_lp]->num_executed_frames++;
		//LPS[current_lp]->lvt = event_ts; //Non esiste il campo lvt, evidentemente sfrutta quello dell'evento bound...lo aggiungiamo?
				
		if(safe){
			delete(nbcalqueue, current_msg->node);/* DELETE_FROM_QUEUE */
		}
		
	#if DEBUG == 0
		unlock(current_lp);
	#else				
		if(!unlock(current_lp))	printf("[%u] ERROR: unlock failed; previous value: %u\n", tid, lp_lock[current_lp]);
	#endif

#if REPORT == 1
	clock_timer_start(queue_op);
#endif
		nbc_prune();
#if REPORT == 1
	statistics_post_data(tid, CLOCK_PRUNE, clock_timer_value(queue_op));
	statistics_post_data(tid, PRUNE_COUNTER, 1);
#endif
		
		//if the LP has ended its life, check the state of the simulation to end it
		if(OnGVT(current_lp, LPS[current_lp]->current_base_pointer) /*|| sim_ended[lp/64]==~(0ULL)*/){
			if(!is_end_sim(current_lp))
				end_sim(current_lp);
			
			if(check_termination())
				__sync_bool_compare_and_swap(&stop, false, true);
		}
		
		if(tid == MAIN_PROCESS) {
			evt_count++;
        
			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printf("[%u] TIME: %f", tid, current_lvt);
				//printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
	}
#if REPORT == 1
	statistics_post_data(tid, CLOCK_LOOP, clock_timer_value(main_loop_time));
#endif

	
	// FIXME: Produces a segmentation fault, probably due to bad memory
	// alignement return by the posix_memalign (!!?)
	//revwin_free(current_lp, current_msg->revwin);

	// Destroy SLAB's structures
	// FIXME
	//reverse_fini();

	// Unmount statistical data
	// FIXME
	//statistics_fini();
	
	if(sim_error){
		printf(COLOR_RED "[%u] Execution ended for an error\n" COLOR_RESET, tid);
	} else if (stop){
		printf(COLOR_GREEN "[%u] Execution ended correctly\n" COLOR_RESET, tid);
	}
}
