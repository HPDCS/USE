#define _GNU_SOURCE

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h> //supporto transazionale
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

//#ifdef REVERSIBLE
//#undef REVERSIBLE
//#endif

//id del processo principale
#define MAIN_PROCESS		0
#define PRINT_REPORT_RATE	1

#define MAX_PATHLEN			512
//#define CACHE_LINE_SIZE 	64


__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;


/* Total number of cores required for simulation */
unsigned int n_cores; //pls cambia nome
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot; //pls cambia nome


/* Commit horizon */
simtime_t gvt = 0;
/* Average time between consecutive events */
simtime_t t_btw_evts = 0.1; //Non saprei che metterci

bool sim_error = false;

void **states;

//used to check termination conditions
bool stop = false;
bool *can_stop;


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
	printf("Thread %d set to CPU no %d\n", tid, tid);
	CPU_ZERO(&mask);
	CPU_SET(tid, &mask);
	int err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}
}


inline void SetState(void *ptr) { //può diventare una macro?
	states[current_lp] = ptr; //è corretto fare questa assegnazione così? Credo andrebbe fatto con una scrittura atomica
}

static void process_init_event(void) { //Spostare direttamente dentro init? Solo per pulizia
	unsigned int i;

	for (i = 0; i < n_prc_tot; i++) {
		current_lp = i;//Si possono eliminare?
		current_lvt = 0;
		//ProcessEvent(i, 0, INIT, NULL, 0, states[i]);
		ProcessEvent(current_lp, current_lvt, INIT, NULL, 0, states[current_lp]);
		queue_deliver_msgs(); //Serve un clean della coda? Secondo me si! No, lo fa direttamente il metodo
	}
}

void init(unsigned int _thread_num, unsigned int lps_num) {
	unsigned int i;
	int lp_lock_ret;

	printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
	n_cores = _thread_num;
	n_prc_tot = lps_num;
	
	states = malloc(sizeof(void *) * n_prc_tot);
	can_stop = malloc(sizeof(bool) * n_prc_tot);
	lp_lock_ret =  posix_memalign((void **)&lp_lock, CACHE_LINE_SIZE, lps_num * CACHE_LINE_SIZE); //  malloc(lps_num * CACHE_LINE_SIZE);
			
	if(states == NULL || can_stop == NULL || lp_lock_ret == 1){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();		
	}
	
	for (i = 0; i < lps_num; i++) {
		lp_lock[i*(CACHE_LINE_SIZE/4)] = 0;
		can_stop[i] = false;
	}

	
#ifndef NO_DYMELOR
	    dymelor_init();
	    printf("Dymelor abilitato\n");
#endif
	statistics_init();
	queue_init();
	numerical_init();
	process_init_event();
}

//potrebbe essere pesante da fare ogni volta? Sostituirlo con una fetch&Add creerebbe troppo conflitti?
//Almeno sostituirlo con una bitmap!
//Nota: ho visto che viene invocato solo a fine esecuzione
bool check_termination(void) {
	unsigned int i;
	bool ret = true;
	for (i = 0; i < n_prc_tot; i++) {
		ret &= can_stop[i]; 
	}
	return ret;
}

//può diventare una macro?
void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	queue_insert(receiver, timestamp, event_type, event_content, event_size);
}

void thread_loop(unsigned int thread_id) {
	
	//unsigned int mode, old_mode, retries;
	//double pending_events;
	revwin_t *window;

	tid = thread_id;
	
	//Set the CPU affinity
	set_affinity(tid);
	//cpu_set_t mask;
	//printf("Thread %d set to CPU no %d\n", tid, tid);
	//CPU_ZERO(&mask);
	//CPU_SET(tid, &mask);
	//int err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	//if(err < 0) {
	//	printf("Unable to set CPU affinity: %s\n", strerror(errno));
	//	exit(-1);
	//}
	lock_init();
	
	reverse_init(REVWIN_SIZE);
	window = revwin_create();

	clock_timer main_loop_time;
	clock_timer_start(main_loop_time);
	
	///* START SIMULATION *///
	while (!stop && !sim_error) {
		
		//mode = retries = 0; //<--possono sparire?

	printf("Start getMinFree\n");
		/// *FETCH* ///
		if (getMinFree() == 0) {
			continue;
		}
	printf("End   getMinFree\n");
execution:		
		queue_clean();
		
		current_lp = current_msg->receiver_id;	//identificatore lp
		current_lvt = current_msg->timestamp;	//local virtual time
				
		//old_mode = mode;
		
		
		if (safe) {
		/// ==== SAFE EXECUTION ==== ///
			//mode = MODE_SAF;
              
			clock_timer event_processing;
			clock_timer_start(event_processing);
              
			ProcessEvent(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, states[current_lp]);
              
			statistics_post_data(tid, EVENTS_SAFE, 1);
			statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(event_processing));
			//sleep(5);//////////////////////////////7
		}
		else {
		/// ==== REVERSIBLE EXECUTION ==== ///
			//mode = MODE_STM;

			// Get the time of the whole STM execution
			clock_timer stm_event_processing;
			clock_timer_start(stm_event_processing);
			statistics_post_data(tid, EVENTS_STM, 1);

			current_msg->revwin = window;
			revwin_reset(current_lp, current_msg->revwin);	//<-da mettere una volta sola ad inizio esecuzione
			
			ProcessEvent_reverse(current_lp, current_lvt, current_msg->type, current_msg->data, current_msg->data_size, states[current_lp]);

			// Get the waiting time
			clock_timer stm_safety_wait;
			clock_timer_start(stm_safety_wait);
			
			do{
				getMinLP(current_lp);
				if(current_msg != new_current_msg /* && current_msg->node != current_msg->node */){
					// Get the time for undo one event
					clock_timer undo_event_processing;
					clock_timer_start(undo_event_processing);
					
					execute_undo_event(current_lp, current_msg->revwin);

					statistics_post_data(tid, CLOCK_UNDO_EVENT, clock_timer_value(undo_event_processing));
					statistics_post_data(tid, ABORT_REVERSE, 1);

					// TODO: handle the reverse cache flush
					//revwin_flush_cache();

					((nbc_bucket_node *)(current_msg->node))->reserved = false; //Si può fare?
					current_msg = new_current_msg;
					safe = new_safe;
					
					goto execution;
					
				}
			}while(!safe);
				
			//Attenzione, ora si sommano i tempi degli eventi squashatu con i relativi eventi eseguiti poi
			statistics_post_data(tid, CLOCK_STM, clock_timer_value(stm_event_processing));

			// Collect the time spend in waiting by a commiting event, only
			statistics_post_data(tid, CLOCK_STM_WAIT, clock_timer_value(stm_safety_wait));
			
			statistics_post_data(tid, COMMITS_STM, 1);
		}

		///* FLUSH */// 
		commit();

		if ((can_stop[current_lp] = OnGVT(current_lp, states[current_lp]))) //va bene cosi?
			stop = check_termination();

		//if(tid == MAIN_PROCESS) {
			evt_count++;

			if ((evt_count - PRINT_REPORT_RATE * (evt_count / PRINT_REPORT_RATE)) == 0) {	
				printf("[%u] TIME: %f", tid, current_lvt);
				printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		//}
	}

	statistics_post_data(tid, CLOCK_LOOP, clock_timer_value(main_loop_time));


	
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
		printf("\n[%u] Execution ended for an error\n\n", tid);
	} else if (stop){
		printf("\n[%u] Execution ended correctly\n\n", tid);
	}
}
