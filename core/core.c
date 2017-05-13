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

#if SPERIMENTAL == 1
	#define gmf getMinFree_internal
	#define gml getMinLP_internal
#else
	#define gmf getMinFree
	#define gml getMinLP
#endif

//id del processo principale
#define MAIN_PROCESS		0
#define PRINT_REPORT_RATE	1000000000000000

#define MAX_PATHLEN			512


__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;


/* Total number of cores required for simulation */
unsigned int n_cores; //pls cambia nome
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot; //pls cambia nome


/* Commit horizon TODO */
 simtime_t gvt = 0;
/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1; //Non saprei che metterci

bool sim_error = false;

void **states;

//used to check termination conditions
bool stop = false;
bool *can_stop;
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
	cpu_set_t mask;	
#if DEBUG == 1
	printf("Thread %d set to CPU no %d\n", tid, tid%(N_CPU));
#endif
	CPU_ZERO(&mask);
	CPU_SET(tid%(N_CPU), &mask);
	int err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}
}

inline void SetState(void *ptr) { //può diventare una macro?
	states[current_lp] = ptr; //è corretto fare questa assegnazione così? Credo andrebbe fatto con una scrittura atomica
}

void init(unsigned int _thread_num, unsigned int lps_num) {
	unsigned int i;
	int lp_lock_ret;

	printf(COLOR_CYAN "\nStarting an execution with %u THREADs, %u LPs :\n", _thread_num, lps_num);
#if SPERIMENTAL == 1
	printf("\t- SPERIMENTAL features enabled.\n");
#endif
#if PREEMPTIVE == 1
	printf("\t- PREEMPTIVE event realease enabled.\n");
#endif
#if DEBUG == 1
	printf("\t- DEBUGDEBUG mode enabled.\n");
#endif
#if MALLOC == 1
	printf("\t- MALLOC enabled.\n");
#else
	printf("\t- DYMELOR enabled.\n");
#endif
#if REPORT == 1
	printf("\t- REPORT prints enabled.\n");
#endif
#if REVERSIBLE == 1
	printf("\t- SPECULATIVE SIMULATION\n");
#else
	printf("\t- CONSERVATIVE SIMULATION\n");
#endif
	printf("\n" COLOR_RESET);

	n_cores = _thread_num;
	n_prc_tot = lps_num;
	
	states = malloc(sizeof(void *) * n_prc_tot);
	can_stop = malloc(sizeof(bool) * n_prc_tot);
	sim_ended = malloc(LP_ULL_MASK_SIZE);
	lp_lock_ret =  posix_memalign((void **)&lp_lock, CACHE_LINE_SIZE, lps_num * CACHE_LINE_SIZE); //  malloc(lps_num * CACHE_LINE_SIZE);
			
	if(states == NULL || can_stop == NULL || sim_ended == NULL || lp_lock_ret == 1){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();		
	}
	
	for (i = 0; i < n_prc_tot; i++) {
		lp_lock[i*(CACHE_LINE_SIZE/4)] = 0;
		sim_ended[lps_num/64] = 0;
		can_stop[i] = false;
	}
	
	if(lps_num%(SIZEOF_ULL*8) != 0){
		for(; i<(LP_ULL_MASK_SIZE*8) ; i++)
			end_sim(i);
	}
	
#if MALLOC == 0
	dymelor_init();
	printf("Dymelor abilitato\nCACHELINESIZE %u\n", CACHE_LINE_SIZE);
#endif
	statistics_init();
	queue_init();
	numerical_init();
	//process_init_event
	for (current_lp = 0; current_lp < n_prc_tot; current_lp++) {
		ProcessEvent(current_lp, 0, INIT, NULL, 0, states[current_lp]); //current_lp = i;
		queue_deliver_msgs(); //Serve un clean della coda? Secondo me si! No, lo fa direttamente il metodo
	}
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

//può diventare una macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	queue_insert(receiver, timestamp, event_type, event_content, event_size);
}

void thread_loop(unsigned int thread_id) {
#if REPORT == 1
	clock_timer main_loop_time,			//OK: cattura il tempo totale di esecuzione sul singolo core...superflup
				queue_min,				//OK: cattura il tempo per fare un estrazione che va a buon fine 		
				event_processing,		//OK: cattura il tempo per processare un evento safe 
				stm_event_processing,	//OK: cattura il tempo per processare un evento in stm (solo evento)
				safety_check_op,		//OK: cattura il tempo per estrarre dalla coda il minimo evento associato ad un LP 
				undo_event_processing,	//OK: cattura il tempo per rollbackare un evento (solo rollback)
				stm_safety_wait;		//OK: cattura il tempo per aspettare che un evento stm diventi safe
#endif
	
	//unsigned int mode, old_mode, retries;
	//double pending_events;
	revwin_t *window;

	tid = thread_id;
	
	//Set the CPU affinity
	set_affinity(tid);
	
	lock_init();
	
	reverse_init(REVWIN_SIZE);
	window = revwin_create();

#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif	
	///* START SIMULATION *///
	while (!stop && !sim_error) {
		//mode = retries = 0; //<--possono sparire?
begin:
		/// *FETCH* ///
#if REPORT == 1
	clock_timer_start(queue_min);
#endif
	if(gmf() == 0){
		continue;
	}
#if REPORT == 1
	statistics_post_data(tid, CLOCK_DEQUEUE, clock_timer_value(queue_min));
	statistics_post_data(tid, EVENTS_FETCHED, 1);
#endif
execution:		
		queue_clean();
		
		current_lp = current_msg->receiver_id;	//identificatore lp
		current_lvt = current_msg->timestamp;	//local virtual time
				
		//old_mode = mode;
				
		if (safe) {
		/// ==== SAFE EXECUTION ==== ///
			//mode = MODE_SAF;
#if REPORT == 1 
			clock_timer_start(event_processing);
#endif
			ProcessEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, states[current_lp]);
#if REPORT == 1              
			statistics_post_data(tid, EVENTS_SAFE, 1);
			statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(event_processing));
#endif		
		}
		
		else {
#if REVERSIBLE == 0 
			((nbc_bucket_node*)current_msg->node)->reserved = false;
			unlock(current_lp);
			continue;
#else
		/// ==== REVERSIBLE EXECUTION ==== ///
			//mode = MODE_STM;
			
			current_msg->revwin = window;
			revwin_reset(current_lp, current_msg->revwin);	//<-da mettere una volta sola ad inizio esecuzione
	#if REPORT == 1 
			clock_timer_start(stm_event_processing);			
			statistics_post_data(tid, EVENTS_STM, 1);
	#endif
			ProcessEvent_reverse(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, states[current_lp]);
	#if REPORT == 1 
			statistics_post_data(tid, CLOCK_STM, clock_timer_value(stm_event_processing));
			clock_timer_start(stm_safety_wait);
	#endif			
			do{
	#if REPORT == 1
				clock_timer_start(safety_check_op);
	#endif
				gml(current_lp);
	#if REPORT == 1
				statistics_post_data(tid, CLOCK_SAFETY_CHECK, clock_timer_value(safety_check_op));
				statistics_post_data(tid, SAFETY_CHECK, 1);
	#endif
				if(current_msg != new_current_msg /* && current_msg->node != current_msg->node */){
	#if REPORT == 1
					clock_timer_start(undo_event_processing);
	#endif					
					execute_undo_event(current_lp, current_msg->revwin);
	#if REPORT == 1
					statistics_post_data(tid, CLOCK_UNDO_EVENT, clock_timer_value(undo_event_processing));
					statistics_post_data(tid, EVENTS_ROLL, 1);
	#endif
					// TODO: handle the reverse cache flush
					//revwin_flush_cache();

					((nbc_bucket_node *)(current_msg->node))->reserved = false;
					current_msg = new_current_msg;
					
					goto execution;
					
				}
	
				else{
					if(stop /*is_end_sim(current_lp)*/ //potrebbe convenire guardare il solo lp, così, in caso di più thread, non si rischia di rimanere appesi
			#if PREEMPTIVE == 1 
				#if REPORT == 1		
						|| (!safe && ((unsafe_events/n_cores) * (avg_tot_clock) > (avg_clock_roll + avg_clock_deq + avg_clock_safe - avg_clock_deqlp)))
				#else 
						|| (unsafe_events > n_cores) //TODO
				#endif
			#endif
					){
						
			#if REPORT == 1
						clock_timer_start(undo_event_processing);
			#endif	
						execute_undo_event(current_lp, current_msg->revwin);
			#if REPORT == 1
						statistics_post_data(tid, CLOCK_UNDO_EVENT, clock_timer_value(undo_event_processing));
						statistics_post_data(tid, EVENTS_STASH, 1);
						statistics_post_data(tid, EVENTS_ROLL, 1);
			#endif
						((nbc_bucket_node*)current_msg->node)->reserved = false;
						unlock(current_lp);
						goto begin;
					}
				}
	
			}while(!safe);
				
	#if REPORT == 1 
			statistics_post_data(tid, CLOCK_STM_WAIT, clock_timer_value(stm_safety_wait));
			statistics_post_data(tid, COMMITS_STM, 1);
	#endif	
#endif		
		}

		///* FLUSH */// 
		commit();
		
		if(OnGVT(current_lp, states[current_lp]) && !is_end_sim(current_lp)){
			end_sim(current_lp);
			if(check_termination())
				__sync_val_compare_and_swap(&stop, false, true);
		}
		
		if(tid == MAIN_PROCESS) {
			evt_count++;
        
			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printf("[%u] TIME: %f", tid, current_lvt);
				printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
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
