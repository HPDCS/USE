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
#include "simtypes.h"
#include "lookahead.h"


//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127

#define MAX_PATHLEN	512

#define HILL_EPSILON_GREEDY	0.05
#define HILL_CLIMB_EVALUATE	500

__thread simtime_t current_lvt = 0;
__thread unsigned int current_lp = 0;
__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;

static simtime_t *current_time_vector;
static bool *current_time_proc;
static unsigned int *current_region;

/* Total number of cores required for simulation */
unsigned int n_cores;
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot;

bool stop = false;
bool sim_error = false;

void **states;

bool *can_stop;

/*
mauro
*/

//lock su LP
int *lp_lock;

//variabili per gestire le segnalazioni di priorità
simtime_t *wait_time;
unsigned int *wait_time_id;
int *wait_time_lk;

//Variabili da tunare durante l'esecuzione per throttling e threshold
double * delta_count;
__thread double reverse_execution_threshold = REV_INIT_THRESH;


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

void SetState(void *ptr) {
	states[current_lp] = ptr;
}

static void process_init_event(void) {
	unsigned int i;

	for (i = 0; i < n_prc_tot; i++) {
		current_lp = i;
		current_lvt = 0;
		ProcessEvent(current_lp, current_lvt, INIT, NULL, 0, states[current_lp]);
		queue_deliver_msgs();
	}
}

void init(unsigned int _thread_num, unsigned int lps_num) {
	unsigned int i;

	if(lps_num < _thread_num) {
		printf("The number of LPs must be grater or equal then the number of threads\n");
		exit(-1);
	}

	printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
	n_cores = _thread_num;
	n_prc_tot = lps_num;
	states = malloc(sizeof(void *) * n_prc_tot);
	can_stop = malloc(sizeof(bool) * n_prc_tot);

	current_time_vector = malloc(sizeof(simtime_t) * n_cores);
	current_time_proc = malloc(sizeof(bool) * n_cores);
    	current_region = malloc(sizeof(unsigned int)*n_cores);

	lp_lock = malloc(sizeof(int) * lps_num);
	wait_time = malloc(sizeof(simtime_t) * lps_num);
	wait_time_id = malloc(sizeof(unsigned int) * lps_num);
	wait_time_lk = malloc(sizeof(int) * lps_num);

	delta_count = malloc(sizeof(double) * n_cores);


	if(states == NULL || can_stop == NULL || lp_lock == NULL || wait_time == NULL || delta_count == NULL ||
		wait_time_id == NULL || wait_time_lk == NULL || current_time_vector == NULL || current_region == NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	for(i = 0; i < n_cores; i++){
        current_time_vector[i] = INFTY;     //processing
        current_region[i] = UINT_MAX;
	delta_count[i] = TROT_INIT_DELTA;
    }

	for (i = 0; i < lps_num; i++) {
		lp_lock[i] = 0;
		wait_time_id[i] = _thread_num + 1;
		wait_time[i] = INFTY;
		wait_time_lk[i] = 0;
		can_stop[i] = false;
	}

	statistics_init();

#ifndef NO_DYMELOR
	    dymelor_init();
#endif

	queue_init();
	numerical_init();
	process_init_event();
}

void execution_time(simtime_t time, unsigned int clp){
    current_time_vector[tid] = time;      //processing
    current_region[tid] = clp;
    current_time_proc[tid] = true;
}

unsigned int check_safety(simtime_t time, unsigned int *e){
    unsigned int i;
    unsigned int events;
    unsigned int effective;

    events = 0;
    effective = 0;

    for(i = 0; i < n_cores; i++){

        if(i!=tid && (
			( (time > (current_time_vector[i]+LOOKAHEAD)) || (time==(current_time_vector[i]+LOOKAHEAD) && tid > i) )
			||
			( (current_lp==current_region[i]) && (time > current_time_vector[i] || (time==current_time_vector[i] && tid > i) ) )
		  )) {
		events++;
		if (current_time_proc[tid])
			effective++;
	}
    }

    *e = effective;
    return events;
}

bool check_termination(void) {
	unsigned int i;
	bool ret = true;
	for (i = 0; i < n_prc_tot; i++) {
		ret &= can_stop[i];
	}
	return ret;
}

void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size) {
	queue_insert(receiver, timestamp, event_type, event_content, event_size);
}


//per ora non viene usato:
//L'idea è di mettere un unico thread con lo scopo di regolare le variabili

void *tuning(void *args){
	unsigned int i;
	unsigned int last_op[n_cores];
	unsigned long long old_mid_time_htm[n_cores];
	unsigned long long mid_time_htm[n_cores];
	double delta[n_cores];

	/* init */
	for(i = 0 ; i < n_cores ; i++){
		last_op[i] = 1;
		//last_op[i] = ~last_op[i];
		delta[i] = 0.1;
	}

	/* loop */
	while(!stop && !sim_error){
		sleep(2);

		for(i = 0; i <	n_cores; i++){
			mid_time_htm[i] = get_useful_time_htm_th(i);
			if(mid_time_htm[i] > old_mid_time_htm[i]){
				last_op[i] = ~last_op[i];
				if(delta[i] > 0.1) delta[i] = delta[i] / 2;
			}
			else{
				if(delta[i] < 0.2) delta[i] = delta[i] * 2;
			}

			if(last_op[i] == 1)
				delta_count[i] += delta[i];
			else
				delta_count[i] -= delta[i];

			if(delta_count[i] < 0)
				delta_count[i] = 0;
			if(delta_count[i] > 1)
				delta_count[i] = 1;

			printf("\t\t\t\t\t\t\t\t\t\t\t\tThroughput of %d old=%llu \n\t\t\t\t\t\t\t\t\t\t\t\t\t\tnew=%llu: \n\t\t\t\t\t\t\t\t\t\t\t\t\t\tdelta : %.3f\n",i ,old_mid_time_htm[i], mid_time_htm[i], delta_count[i]);

			old_mid_time_htm[i] = mid_time_htm[i];
		}
	}

	printf("Esecuzione del tuning terminata\n");

	pthread_exit(NULL);
}

bool check_waiting(){
	return (wait_time[current_lp] < current_lvt || (wait_time[current_lp] == current_lvt && wait_time_id[current_lp] < tid));
}

int get_lp_lock(unsigned int mode, unsigned int bloc) {
//mode  0=condiviso      1=esclusivo
//bloc  0=non bloccante  1=bloccante
	int old_lk;
	simtime_t old_tm;
	unsigned int old_tm_id;

	do{
		if(check_waiting()){
			continue;
		}
		///ESCLUSIVO
		if (mode==1){
			if ( (old_lk = lp_lock[current_lp]) == 0 ) {
				if (__sync_val_compare_and_swap(&lp_lock[current_lp], 0, -1) == 0){
					return 1;
				}

			}
		///CONDIVISO
		} else if (mode==0){
sh_lk:			if ( (old_lk = lp_lock[current_lp]) >= 0 ) {
				if (__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk + 1) == old_lk){
					return 1;
				}
				else
					goto sh_lk;
			}
		}
		
		///PRENOTAZIONE
		//voglio prendere il lock ma non posso perche non e libero
		if(wait_time_id[current_lp]==tid)
			continue; //se c'e il mio id, vuol dire che non e stato aggiornato (o, se lo stanno aggiornando, me ne accorgo al giro successivo)

		while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
			while (wait_time_lk[current_lp] == 1) ;

		old_tm_id = wait_time_id[current_lp];
		old_tm = wait_time[current_lp];	//puo avere senso leggerli prima e prendere il lock e rileggerli solo se necessario?

		if (current_lvt < old_tm || (current_lvt == old_tm && tid < old_tm_id)) {
			wait_time[current_lp] = current_lvt;
			wait_time_id[current_lp] = tid;
		}
		__sync_lock_release(&wait_time_lk[current_lp]);

		while ( bloc == 1 &&  check_waiting() );//aspetto di diventare il min

	} while(bloc);

	return 0;
}


void release_waiting_ticket(){
	if (wait_time_id[current_lp] == tid) { //rifaccio questo controllo anche prima del lock, in modo da evitarlo nel caso non sia necessario
		while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
			while (wait_time_lk[current_lp] == 1) ;

		if (wait_time_id[current_lp] == tid) {
			wait_time_id[current_lp] = UINT_MAX;
			wait_time[current_lp] = INFTY;
		}
		__sync_lock_release(&wait_time_lk[current_lp]);

	}
}


void release_lp_lock() {
	int old_lk = lp_lock[current_lp];

	//metto questo fuori dal ciclo per evitarmi ogni volta il controllo
	if (old_lk == -1) {
		lp_lock[current_lp] = 0;	///posso anche non fare il cas perche tanto solo uno alla volta rilascia il lock esclusivo
	} else {
		do {
			if (__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk - 1) == old_lk) {
				break;
			}
			old_lk = lp_lock[current_lp];	//mettendono alla fine risparmio una lettura
		} while (1);
	}
	release_waiting_ticket();
}


void thread_loop(unsigned int thread_id, int incarnation) {

	unsigned int status, safe, mode, old_mode, retries;
	unsigned long long tick_count, tick_base, mid_time_htm, mid_time_stm;
	double pending_events;
	bool retry_event;
	revwin_t *window;
	cpu_set_t mask;

	unsigned int effective;
	unsigned int affinity;

	tid = thread_id;

	if(incarnation > 0) {
		thread_loop(thread_id, incarnation-1);
		return;
	}

	// Set the CPU affinity
	affinity = (tid + 1)%8;
	CPU_ZERO(&mask);
	CPU_SET(affinity, &mask);
	printf("Thread %d set to CPU no %d\n", tid, affinity);
	int err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}

	reverse_init(REVWIN_SIZE);

	window = revwin_create();

	while (!stop && !sim_error) {

		mode = retries = 0;

		/*FETCH*/
		if (queue_min() == 0) {
			execution_time(INFTY,UINT_MAX);
			continue;
		}

		current_msg.revwin = window;

		//lvt ed lp dell'evento corrente
		current_lp = current_msg.receiver_id;	//identificatore lp
		current_lvt = current_msg.timestamp;	//local virtual time

		while (1) {

			old_mode = mode;

			// Get statistics about the total attempts done so far
			// (this value should be the same of TOTAL_EVENTS)
			statistics_post_data(tid, TOTAL_ATTEMPTS, 1);

			safe = check_safety(current_lvt, &effective); //effective può essere tolto

			// Save the number of threads holding newer events
			statistics_post_data(tid, PRIO_THREADS, safe);

			//compute the average number of events between the commit horizon and my currebt ts
			pending_events = (current_lvt - gvt)/t_btw_evts - 1;
			if(pending_events<0) pending_events=0;

			// Save the computed number of pending events from the current LVT
			statistics_post_data(tid, PENDING_EVENTS, pending_events);

/// ==== ESECUZIONE SAFE ====
///non ci sono problemi quindi eseguo normalmente*/

			if (safe == -1) {
				mode = MODE_SAF;

#ifdef REVERSIBLE
				get_lp_lock(0, 1);
#endif
				clock_timer event_processing;
				clock_timer_start(event_processing);

				ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

				statistics_post_data(tid, EVENTS_SAFE, 1);
				statistics_post_data(tid, CLOCK_SAFE, clock_timer_value(event_processing));
#ifdef REVERSIBLE
				release_lp_lock();
#endif

			}

/// ==== ESECUZNE HTM ====
///non sono safe quindi ricorro ad eseguire eventi in htm*/
			else if (pending_events < reverse_execution_threshold) {
				mode = MODE_HTM;
				statistics_post_data(tid, EVENTS_HTM, 1);

				if(mode == old_mode) retries++;
//				if(retries!=0 && retries%(1000000)==0)
//					printf("[TH%d] HTM %d RETRIES, event %.2f on LP%d with CS %u\n", tid, retries, current_lvt, current_lp, safe);
#ifdef REVERSIBLE
				if(get_lp_lock(0, 0) == 0){
					continue;
				}
#endif
				// Get the time of the whole exectution of an event in HTM
				clock_timer htm_event_processing;
				clock_timer_start(htm_event_processing);

				tick_count = (unsigned long long)(pending_events * tick_base);		//x

				if ((status = _xbegin()) == _XBEGIN_STARTED) {

					ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
#ifdef THROTTLING
					// Get the time of the whole time spent in throttling
					// (NOTE! only for future committed events)
					clock_timer htm_throttling;
					clock_timer_start(htm_throttling);

					/* TROTTLING */
					if(delta_count[tid] > 0){
						tick_count += CLOCK_READ();
						while(CLOCK_READ() < tick_count);
					}
#endif
					if (check_safety(current_lvt, &effective) == 0) {
						_xend();

						statistics_post_data(tid, COMMITS_HTM, 1);
						statistics_post_data(tid, CLOCK_HTM_THROTTLE, clock_timer_value(htm_throttling));
						statistics_post_data(tid, CLOCK_HTM, clock_timer_value(htm_event_processing));

#ifdef REVERSIBLE
						release_lp_lock();
#endif


					} else {
						_xabort(_ROLLBACK_CODE);
					}
				} else {

					statistics_post_data(tid, ABORT_TOTAL, 1);

					if (_XABORT_CODE(status) == _ROLLBACK_CODE) {
						statistics_post_data(tid, ABORT_UNSAFE, 1);

					} else {
						if (status & _XABORT_RETRY){
							statistics_post_data(tid, ABORT_RETRY, 1);
						}
						if (status & _XABORT_CONFLICT){
								statistics_post_data(tid, ABORT_CONFLICT, 1);
						}
						if (status & _XABORT_CAPACITY) {
							statistics_post_data(tid, ABORT_CACHEFULL, 1);
						}
						if (status & _XABORT_DEBUG) {
							statistics_post_data(tid, ABORT_DEBUG, 1);
						}
						if (status & _XABORT_NESTED) {
							statistics_post_data(tid, ABORT_NESTED, 1);
						}
					}
#ifdef REVERSIBLE
					release_lp_lock();
#endif
					statistics_post_data(tid, CLOCK_HTM, clock_timer_value(htm_event_processing));
					if(retries < 1 && status & _XABORT_RETRY)
						continue;
					else{
						statistics_post_data(tid, TOTAL_ATTEMPTS, 1);
						goto reversible;
					}
				}

			}

/// ==== ESECUZIONE REVERSIBILE ====
///mi sono allontanato molto dal GVT, quindi preferisco un esecuzione reversibile*/
#ifdef REVERSIBLE
			else {
reversible:
				mode = MODE_STM;
				statistics_post_data(tid, EVENTS_STM, 1);

				//if(old_mode == mode) retries++;
				//if(retries>0 && retries%(100) == 0) printf("STM RETRIES: %d\n", retries);

				if(get_lp_lock(1, 0)==0){
					//printf("The unprevedible it si happening\n");
					continue; //Se non riesco a prendere il lock riparto da capo perche magari a questo giro rientro in modalita transazionale
				}


				// Get the time of the whole STM execution
				clock_timer stm_event_processing;
				clock_timer_start(stm_event_processing);

				revwin_reset(current_lp, current_msg.revwin);	//<-da mettere una volta sola ad inizio esecuzione
				ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

				retry_event = false;

				// Get the waiting time
				clock_timer stm_safety_wait;
				clock_timer_start(stm_safety_wait);

				while (check_safety(current_lvt, &effective) > 0) {
					if ( check_waiting() ) {

						statistics_post_data(tid, CLOCK_STM_WAIT, clock_timer_value(stm_safety_wait));

						// Get the time for undo one event
						clock_timer undo_event_processing;
						clock_timer_start(undo_event_processing);

						execute_undo_event(current_lp, current_msg.revwin);

						statistics_post_data(tid, CLOCK_UNDO_EVENT, clock_timer_value(undo_event_processing));
						statistics_post_data(tid, ABORT_REVERSE, 1);

						// TODO: handle the reverse cache flush
						//revwin_flush_cache();

						queue_clean();

						// Reversible event must be retried, no other events can be
						// fetched until the current one will not be processed
						retry_event = true;
						break;
					}
				}

				if(!retry_event) {
					// Collect the time spend in waiting by a commiting event, only
					statistics_post_data(tid, CLOCK_STM_WAIT, clock_timer_value(stm_safety_wait));
				}

				statistics_post_data(tid, CLOCK_STM, clock_timer_value(stm_event_processing));

				release_lp_lock();

				// The reversible event was not safe and someone else has a less event's timestamp,
				// therefore the whole event has been just reversed and we must retry it from
				// the very beginning (i.e. safe->htm->stm).
				if (retry_event)
					continue;

				// If we are here, this means that the reversible event is now become safe and thus,
				// it will be soon commited
				statistics_post_data(tid, COMMITS_STM, 1);
			}
#endif

			break;
		}

		/*FLUSH*/ 
		flush();
		current_time_proc[tid] = false;
//		printf("[TH%d] Event %.3f on LP%d committed\n", tid, current_lvt, current_lp);

		if ((can_stop[current_lp] = OnGVT(current_lp, states[current_lp]))) //va bene cosi?
			stop = check_termination();


		evt_count++;

		if(evt_count%1000==0){
			mid_time_htm = get_useful_time_htm();
			mid_time_stm = get_useful_time_stm();
			if(mid_time_htm > mid_time_stm){
				reverse_execution_threshold-=0.25;
				if(reverse_execution_threshold < 0)
					 reverse_execution_threshold = 0;
			}
			else{
				reverse_execution_threshold+=0.25;
			}
			if(evt_count%100000==0)
				printf("[TH%u] threshold setted to %.2f - ratio htm/stm: %f\n",tid, reverse_execution_threshold, (double)mid_time_htm/(double)mid_time_stm);

			tick_base = (unsigned long long)get_time_of_an_event() * delta_count[tid];
			//printf("[TH%d]tick:base %llu\n", tid, get_time_of_an_event());

			//tick_count = (unsigned long)(pending_events*pending_events*tick_base);	//x*x
			//tick_count = (unsigned long)((1/(4-pending_events))*tick_base);	//1/(4-x)
			//tick_count = (unsigned long)((10 / (4 - 0.5*pending_events))*tick_base);		// 10 / (4 - 0.5x)
		}

		if(tid == _MAIN_PROCESS) {

			if ((evt_count - 100000 * (evt_count / 100000)) == 0) {	//10000
				printf("[%u] TIME: %14.3f", tid, current_lvt);
				printf(" \tsafety=%12u \ttransactional=%12u \treversible=%12u\n", thread_stats[tid].events_safe, thread_stats[tid].commits_htm, thread_stats[tid].commits_stm);
			}
		}
	}

	execution_time(INFTY,-1);
	
	// FIXME: Produces a segmentation fault, probably due to bad memory
	// alignement return by the posix_memalign (!!?)
//	revwin_free(current_lp, current_msg.revwin);

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
