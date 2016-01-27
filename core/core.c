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
#include "message_state.h"
#include "simtypes.h"


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
double delta_count = TROT_INIT_DELTA;
unsigned int reverse_execution_threshold = REV_INIT_THRESH;



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

void throttling(unsigned int events) {
	unsigned long long tick_count;
	
	tick_count = CLOCK_READ() + events * (system_stats[tid].clock_safe * delta_count); 
	while (true) {
		if (CLOCK_READ() > tick_count)
			break;
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

	printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
	n_cores = _thread_num;
	n_prc_tot = lps_num;
	states = malloc(sizeof(void *) * n_prc_tot);
	can_stop = malloc(sizeof(bool) * n_prc_tot);

	lp_lock = malloc(sizeof(int) * lps_num);
	wait_time = malloc(sizeof(simtime_t) * lps_num);
	wait_time_id = malloc(sizeof(unsigned int) * lps_num);
	wait_time_lk = malloc(sizeof(int) * lps_num);
	
		
	if(states == NULL || can_stop == NULL || lp_lock == NULL || wait_time == NULL || 
		wait_time_id == NULL || wait_time_lk == NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();		
	}
	
	
	for (i = 0; i < lps_num; i++) {
		lp_lock[i] = 0;
		wait_time_id[i] = _thread_num + 1;
		wait_time[i] = INFTY;
		wait_time_lk[i] = 0;
		can_stop[i] = false; //<--non c'era, serve? secondo me si
	}

	statistics_init();

#ifndef NO_DYMELOR
	    dymelor_init();
#endif

	queue_init();
	message_state_init();
	numerical_init();

	process_init_event();
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
	unsigned int committed, old_committed, throughput, old_throughput, last_op, field, i;
	old_throughput = 0;
	last_op = 1;
	//field = 1; // 0 sta per delta_count ed 1 sta per reverse_execution_threshold 
	double delta = 0.1;
	
	while(!stop && !sim_error){
		sleep(3);
		throughput = 0;
		committed = 0;
		
		for(i = 0; i <	n_cores; i++)
			//committed += (committed_safe[i] + committed_htm[i] + committed_reverse[i]); //totale transazioni commitatte
			committed = system_stats[tid].events_safe + system_stats[tid].commits_htm + system_stats[tid].commits_htm;
		throughput = committed - old_committed;
	
		if(throughput < old_throughput){//poichè ho visto un peggioramento, inverto la direzione
			last_op=~last_op;
			if(delta>0.025) delta = delta / 2;
		}
		else{
			if(delta<0.2) delta = delta * 2;
		}
			
		if(last_op==1)
			delta_count+=delta;
		else 
			delta_count-=delta;
			
		if(delta_count<0) 
			delta_count=0;
		if(delta_count>1)
			delta_count=1;
				
		printf("\t\t\t\t\t\t\t\t\t\t\t\tThroughput \told=%u \n\t\t\t\t\t\t\t\t\t\t\t\t\t\tnew=%u: \n\t\t\t\t\t\t\t\t\t\t\t\t\t\tdelta : %.3f\n", old_throughput, throughput, delta_count);
		
		old_committed = committed;
		old_throughput = throughput;
	}

	printf("Esecuzione del tuning terminata\n");
	
	pthread_exit(NULL);
}



double double_cas(double *addr, double old_val, double new_val) {
	long long res;

	res = __sync_val_compare_and_swap(UNION_CAST(addr, long long *), UNION_CAST(old_val, long long), UNION_CAST(new_val, long long));

	return UNION_CAST(res, double);
}

int check_waiting(){
	return (wait_time[current_lp] < current_lvt || (wait_time[current_lp] == current_lvt && wait_time_id[current_lp] < tid));
}

int get_lp_lock(unsigned int mode, unsigned int bloc) {
//mode  0=condiviso      1=esclusivo
//bloc  0=non bloccante  1=bloccante
	int old_lk;
	simtime_t old_tm;
	unsigned int old_tm_id;

	do{
		///ESCLUSIVO
		if (mode && (old_lk = lp_lock[current_lp]) == 0) {
			if (__sync_val_compare_and_swap(&lp_lock[current_lp], 0, -1) == 0)
				return 1;

		///CONDIVISO
		}else if (!mode && (old_lk = lp_lock[current_lp]) >= 0) {
			if (__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk + 1) == old_lk)
				return 1;

		///PRENOTAZIONE
		}else {	//voglio prendere il lock ma non posso perche non è libero
			if(wait_time_id[current_lp]==tid)
				break; //se c'è il mio id, vuol dire che non è stato aggiornato (o, se lo stanno aggiornando, me ne accorgo al giro successivo)

			while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
				while (wait_time_lk[current_lp]) ;
				
			old_tm_id = wait_time_id[current_lp];
			old_tm = wait_time[current_lp];	//può avere senso leggerli prima e prendere il lock e rileggerli solo se necessario?
			
			if (current_lvt < old_tm || (current_lvt == old_tm && tid < old_tm_id)) {
				wait_time[current_lp] = current_lvt;
				wait_time_id[current_lp] = tid;
				__sync_lock_release(&wait_time_lk[current_lp]);
				break;
			}
			__sync_lock_release(&wait_time_lk[current_lp]);
			
			if(bloc)	
				while ( check_waiting() );//aspetto di diventare il min
		}
		
	}while(bloc);
	
	return 0;

}


void release_lp_lock() {
	int old_lk = lp_lock[current_lp];

	//metto questo fuori dal ciclo per evitarmi ogni volta il controllo
	if (old_lk == -1) {
		lp_lock[current_lp] = 0;	///posso anche non fare il cas perche tanto solo uno alla volta rilascia il lock esclusivo
	} else {
		do {
			if (__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk - 1) == old_lk) {
				return;
			}
			old_lk = lp_lock[current_lp];	//mettendono alla fine risparmio una lettura
		} while (1);
	}
}

void release_waiting_ticket(){
	if (wait_time_id[current_lp] == tid) { //rifaccio questo controllo anche prima del lock, in modo da evitarlo nel caso non sia necessario
		while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
			while (wait_time_lk[current_lp]) ;

		if (wait_time_id[current_lp] == tid) {
			wait_time_id[current_lp] = UINT_MAX;
			wait_time[current_lp] = INFTY;
		}
		__sync_lock_release(&wait_time_lk[current_lp]);

	}
}

void thread_loop(unsigned int thread_id) {
	
	unsigned int status, pending_events, mode, old_mode, retries;
	unsigned long long t_pre, t_post, t_pre2, t_post2;// per throttling
	bool retry_event;
	revwin_t *window;

	tid = thread_id;
	
	reverse_init(REVWIN_SIZE);

	window = revwin_create();

	while (!stop && !sim_error) {
		
		mode = retries = 0;

		/*FETCH*/
		if (queue_min() == 0) {
			execution_time(INFTY,-1);
			continue;
		}
		
		current_msg.revwin = window;
		
		//lvt ed lp dell'evento corrente
		current_lp = current_msg.receiver_id;	//identificatore lp
		current_lvt = current_msg.timestamp;	//local virtual time
		
		while (1) {
			
			old_mode = mode;
			
/// ==== ESECUZIONE SAFE ====
///non ci sono problemi quindi eseguo normalmente*/
			if (0 && (pending_events = check_safety(current_lvt)) == 0) {  //if ((pending_events = check_safety_lookahead(current_lvt)) == 0) {
				mode = MODE_SAF;
#ifdef REVERSIBLE
				get_lp_lock(0, 1);
#endif
				timer event_processing;
				timer_start(event_processing);

				ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

				statistics_post_data(tid, EVENTS_SAFE, 1);
				statistics_post_data(tid, CLOCK_SAFE, (double)timer_value_micro(event_processing));

#ifdef REVERSIBLE
				release_lp_lock();
#endif

			}

/// ==== ESECUZNE HTM ====
///non sono safe quindi ricorro ad eseguire eventi in htm*/
			else if (pending_events < reverse_execution_threshold) {
				mode = MODE_HTM;
				if(mode == old_mode) retries++;
				if(retries!=0 && retries%(100)==0) printf("++++HO FATTO %d tentativi\n", retries);
				

				statistics_post_data(tid, EVENTS_HTM, 1);

#ifdef REVERSIBLE
				get_lp_lock(0, 1);
#endif

				timer event_htm_processing;
				timer_start(event_htm_processing);

				if ((status = _xbegin()) == _XBEGIN_STARTED) {

					ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

#ifdef THROTTLING
					timer htm_throttling;
					timer_start(htm_throttling);

					throttling(pending_events);

					statistics_post_data(tid, CLOCK_HTM_THROTTLE, (double)timer_value_micro(htm_throttling));
#endif

					if (check_safety(current_lvt) == 0) {
						_xend();
						
						statistics_post_data(tid, COMMITS_HTM, 1);

#ifdef REVERSIBLE
						release_lp_lock();
#endif

					} else {
						_xabort(_ROLLBACK_CODE);

					}
				} else {	//se il commit della transazione fallisce, finisce qui
					if (status & _XABORT_CAPACITY) {
						statistics_post_data(tid, ABORT_CACHEFULL, 1);
					}
					else if (status & _XABORT_DEBUG) {
						statistics_post_data(tid, ABORT_DEBUG, 1);
					}
					else if (status & _XABORT_CONFLICT) {
						statistics_post_data(tid, ABORT_CONFLICT, 1);
					}
					else if (_XABORT_CODE(status) == _ROLLBACK_CODE) {
						statistics_post_data(tid, ABORT_UNSAFE, 1);
					}
					else {
						statistics_post_data(tid, ABORT_GENERIC, 1);
					}

#ifdef REVERSIBLE
					release_lp_lock();
#endif

				statistics_post_data(tid, CLOCK_HTM, (double)timer_value_micro(event_htm_processing));

					continue;
				}

			}

/// ==== ESECUZIONE REVERSIBILE ====
///mi sono allontanato molto dal GVT, quindi preferisco un esecuzione reversibile*/
#ifdef REVERSIBLE
			else {
reversible:
				mode = MODE_STM;

				statistics_post_data(tid, EVENTS_STM, 1);

				timer event_stm_processing;
				timer_start(event_stm_processing);

				if(get_lp_lock(1, 0)==0)
					continue; //Se non riesco a prendere il lock riparto da capo perche magari a questo giro rientro in modalità transazionale

				revwin_reset(current_lp, current_msg.revwin);	//<-da mettere una volta sola ad inizio esecuzione
				ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
				
				retry_event = false;

				timer stm_safety_wait;
				timer_start(stm_safety_wait);

				while (check_safety_lookahead(current_lvt) > 0) {
					if ( check_waiting() ) {

						statistics_post_data(tid, CLOCK_STM_WAIT, (double)timer_value_micro(stm_safety_wait));
						
						timer undo_event_processing;
						timer_start(undo_event_processing);
						
						execute_undo_event(current_lp, current_msg.revwin);

						statistics_post_data(tid, CLOCK_UNDO_EVENT, (double)timer_value_micro(undo_event_processing));
						statistics_post_data(tid, ABORT_REVERSE, 1);

						// TODO: handle the reverse cache flush
						//revwin_flush_cache();

						queue_clean();

						retry_event = true;
						break;
					}
				}
				
				release_lp_lock();
				
				if (retry_event)
					continue;				
			}
#endif

			break;
		}

		release_waiting_ticket();

		/*FLUSH*/ 
		flush();
		
		if ((can_stop[current_lp] = OnGVT(current_lp, states[current_lp]))) //va bene cosi?
			stop = check_termination();

		if(tid == _MAIN_PROCESS) {
		evt_count++;
			if ((evt_count - 100000 * (evt_count / 100000)) == 0) {	//10000
				printf("[%u] TIME: %f", tid, current_lvt);
				printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", system_stats[tid].events_safe, system_stats[tid].commits_htm, system_stats[tid].commits_stm);
			}
		}
	}

	execution_time(INFTY,-1);
	
	revwin_free(current_lp, current_msg.revwin);

	// Destroy SLAB's structures
	// FIXME: does not work!
	//reverse_fini();
	
	if(sim_error){
		printf("\n[%u] Execution ended for an error\n\n", tid);
	} else if (stop){
		printf("\n[%u] Execution ended correctly\n\n", tid);
	}
}
