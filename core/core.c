#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ROOT-Sim.h>
#include <dymelor.h>
#include <numerical.h>
#include <timer.h>

#include "core.h"
#include "queue.h"
#include "message_state.h"
#include "simtypes.h"

#include "reverse.h"

#define THROTTLING

//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127

#define MAX_PATHLEN	512

#define HILL_EPSILON_GREEDY	0.05
#define HILL_CLIMB_EVALUATE	500
#define DELTA 500		// tick count
#define HIGHEST_COUNT	5
__thread int delta_count = 0;
__thread double abort_percent = 1.0;

__thread simtime_t current_lvt = 0;

__thread unsigned int current_lp = 0;

__thread unsigned int tid = 0;

__thread unsigned long long evt_count = 0;
__thread unsigned long long evt_try_count = 0;
__thread unsigned long long abort_count_conflict = 0, abort_count_safety = 0, abort_count_reverse = 0;

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
int *lp_lock;

simtime_t *wait_time;
unsigned int *wait_time_id;
int *wait_time_lk;

unsigned int reverse_execution_threshold = 0;	//ho messo un valore a caso, ma sar� da fissare durante l'inizializzazione

#define FINE_GRAIN_DEBUG

//forse da cancellare
unsigned int * committed_safe;
unsigned int * committed_htm;
unsigned int * committed_reverse;
unsigned int * abort_unsafety;
unsigned int * abort_conflict;
unsigned int * abort_waiting;

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
	long long tick_count;
	//register int i;

	if (delta_count == 0)
		return;
	//for(i = 0; i < 1000; i++);

	tick_count = CLOCK_READ();
	while (true) {
		if (CLOCK_READ() > (tick_count + events * DELTA * delta_count))
			break;
	}
}

void hill_climbing(void) {
	if ( ((double)abort_unsafety[tid] / (double)evt_count) < abort_percent && delta_count < HIGHEST_COUNT) {
		delta_count++;
		//printf("Incrementing delta_count to %d\n", delta_count);
	} else {
/*		if(random() / RAND_MAX < HILL_EPSILON_GREEDY) {
			delta_count /= (random() / RAND_MAX) * 10 + 1;
		}
*/ }

	abort_percent = (double)abort_unsafety[tid] / (double)evt_count;
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
	
	//forse non servono
	committed_safe = malloc(sizeof(unsigned int) * n_cores);
	committed_htm = malloc(sizeof(unsigned int) * n_cores);
	committed_reverse = malloc(sizeof(unsigned int) * n_cores);
	abort_unsafety = malloc(sizeof(unsigned int) * n_cores);
	abort_waiting = malloc(sizeof(unsigned int) * n_cores);
	abort_conflict = malloc(sizeof(unsigned int) * n_cores);
		
	if(states == NULL || can_stop == NULL || lp_lock == NULL || wait_time == NULL || 
		wait_time_id == NULL || wait_time_lk == NULL ||
		committed_htm == NULL || committed_reverse == NULL || committed_safe == NULL || 
		abort_conflict == NULL || abort_unsafety == NULL || abort_waiting == NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();		
	}
	
	for (i = 0; i < n_cores; i++) {
		committed_safe[i] = 0;
		committed_htm[i] = 0;
		committed_reverse[i] = 0;
		abort_unsafety[i] = 0;
		abort_waiting[i] = 0;
		abort_conflict[i] = 0;
	}
	
	for (i = 0; i < lps_num; i++) {
		lp_lock[i] = 0;
		wait_time_id[i] = _thread_num + 1;
		wait_time[i] = INFTY;
		wait_time_lk[i] = 0;
		can_stop[i] = false; //<--non c'era, serve? secondo me si
	}

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

void print_report(void){
	unsigned int i;;
	unsigned int tot_committed_safe = 0;
	unsigned int tot_committed_htm = 0;
	unsigned int tot_committed_reverse = 0;
	unsigned int tot_abort_unsafety = 0;
	unsigned int tot_abort_conflict = 0;
	unsigned int tot_abort_waiting = 0;
			
	printf("\n\n|\tTID\t|\tSafe\t|\tHtm\t|\tRevers\t|\tTOTAL\t|\n");
	printf("|---------------|---------------|---------------|---------------|---------------|\n");
	for(i = 0; i < n_cores; i++){
		printf("|\t[%u]\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\n", i, committed_safe[i], committed_htm[i], committed_reverse[i], (committed_safe[i]+committed_htm[i]+committed_reverse[i]) );
		tot_committed_safe += committed_safe[i];
		tot_committed_htm += committed_htm[i];
		tot_committed_reverse += committed_reverse[i];
	}
	printf("|---------------|---------------|---------------|---------------|---------------|\n");
	printf("|\tTOT\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\n", tot_committed_safe, tot_committed_htm, tot_committed_reverse, (tot_committed_safe+tot_committed_htm+tot_committed_reverse) );
	
	printf("\n\n|\tTID\t|\tUnsafe\t|\tConfl\t|\tWait\t|\tTOTAL\t|\n");
	printf("|---------------|---------------|---------------|---------------|---------------|\n");
	for(i = 0; i < n_cores; i++){
		printf("|\t[%u]\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\n", i, abort_unsafety[i], abort_conflict[i], abort_waiting[i], (abort_unsafety[i]+abort_conflict[i]+abort_waiting[i]) );
		tot_abort_unsafety += abort_unsafety[i];
		tot_abort_conflict += abort_conflict[i];
		tot_abort_waiting += abort_waiting[i];
	}
	printf("|---------------|---------------|---------------|---------------|---------------|\n");
	printf("|\tTOT\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\n\n", tot_abort_unsafety, tot_abort_conflict, tot_abort_waiting, (tot_abort_unsafety+tot_abort_conflict+tot_abort_waiting) );
}

//per ora non viene usato:
//L'idea � di mettere un unico thread con lo scopo di regolare le variabili
void *tuning(void *args){
	unsigned int committed, old_committed, throughput, old_throughput, last_op, i;
	
	
	while(!stop && !sim_error){
		sleep(1);
		throughput=0;
		for(i = 0; i <	n_cores; i++)
			committed += (committed_safe[i] + committed_htm[i] + committed_reverse[i]); //totale transazioni commitatte
		throughput = committed - old_committed;
		if(throughput > old_throughput){
			if(last_op==1)
				delta_count++;
			else
				delta_count--;
		}else if(throughput < old_throughput){
			if(last_op==1){
				delta_count--;
				last_op=0;
			}
			else{
				delta_count++;
				last_op=1;
			}	
		}
		old_committed = committed;
		old_throughput = throughput;
		
		printf("Delta ha raggiunto il valore %u", delta_count);
	}
	
	
	pthread_exit(NULL);
}

/**
* @author Mauro Ianni
*/

double double_cas(double *addr, double old_val, double new_val) {
	long long res;

	res = __sync_val_compare_and_swap(UNION_CAST(addr, long long *), UNION_CAST(old_val, long long), UNION_CAST(new_val, long long));

	return UNION_CAST(res, double);
}

/**
* @author Mauro Ianni
*/
int get_lp_lock(unsigned int mode, unsigned int bloc) {
//mode  0=condiviso      1=esclusivo
//bloc  0=non bloccante  1=bloccante
	int old_lk;
	simtime_t old_tm;
	unsigned int old_tm_id;
	//cosi facendo il controllo sul "mode" viene fatto una volta sola

///LOCK ESCLUSIVO
	if (mode) {
		do {
			if ((old_lk = lp_lock[current_lp]) == 0) {
				if (__sync_val_compare_and_swap(&lp_lock[current_lp], 0, -1) == 0) {
					return 1;
				}
			} else {	//voglio prendere il lock esclusivo ma non posso perche non � libero
				while (1) {	//PER VEDERE VECCHIA IMPLEMENTAZIONE, ANDARE ALLE VERSIONI PRECEDENTE AL 19 LUGLIO
					if(wait_time_id[current_lp]==tid)
						break; //se c'� il mio id, vuol dire che non � stato aggiornato (o, se lo stanno aggiornando, me ne accorgo al giro successivo)
						
					while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
						while (wait_time_lk[current_lp]) ;

					old_tm_id = wait_time_id[current_lp];
					old_tm = wait_time[current_lp];	//pu� avere senso leggerli prima e prendere il lock e rileggerli solo se necessario?

					if (current_lvt < old_tm || (current_lvt == old_tm && tid < old_tm_id)) {
						wait_time[current_lp] = current_lvt;
						wait_time_id[current_lp] = tid;
						__sync_lock_release(&wait_time_lk[current_lp]);
						break;
					}
					__sync_lock_release(&wait_time_lk[current_lp]);

					while (wait_time[current_lp] < current_lvt || (wait_time[current_lp] == current_lvt && tid > wait_time_id[current_lp])) ;	//aspetto di diventare il min

				}
			}
		} while (bloc);
	}

///LOCK CONDIVISO
	else {
		do {
			if ((old_lk = lp_lock[current_lp]) >= 0) {
				if (__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk + 1) == old_lk) {
					return 1;
				}

			} else {	//voglio prendere il lock condiviso ma non posso perche non � libero
				while (1) {	//PER VEDERE VECCHIA IMPLEMENTAZIONE, ANDARE ALLE VERSIONI PRECEDENTE AL 19 LUGLIO
					if(wait_time_id[current_lp]==tid)
						break; //se c'� il mio id, vuol dire che non � stato aggiornato (o, se lo stanno aggiornando, me ne accorgo al giro successivo)
						
					while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
						while (wait_time_lk[current_lp]) ;

					old_tm_id = wait_time_id[current_lp];
					old_tm = wait_time[current_lp];	//pu� avere senso leggerli prima e prendere il lock e rileggerli solo se necessario?

					if (current_lvt < old_tm || (current_lvt == old_tm && tid < old_tm_id)) {
						wait_time[current_lp] = current_lvt;
						wait_time_id[current_lp] = tid;
						__sync_lock_release(&wait_time_lk[current_lp]);
						break;
					}
					__sync_lock_release(&wait_time_lk[current_lp]);

					while (wait_time[current_lp] < current_lvt || (wait_time[current_lp] == current_lvt && tid > wait_time_id[current_lp])) ;	//aspetto di diventare il min

				}
			}
		} while (bloc);
	}

	return 0;		//arriva qui solo se il lock � non bloccante e non ha potuto prenderlo

}

/**
* @author Mauro Ianni
*/
void release_lp_lock() {

	int old_lk;

	if (wait_time_id[current_lp] == tid) {
		while (__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
			while (wait_time_lk[current_lp]) ;

		if (wait_time_id[current_lp] == tid) {
			wait_time_id[current_lp] = n_cores;
			wait_time[current_lp] = INFTY;
		}
		__sync_lock_release(&wait_time_lk[current_lp]);

	}
	old_lk = lp_lock[current_lp];

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

void thread_loop(unsigned int thread_id) {
	unsigned int status, pending_events;

	bool continua;

	revwin *window; //fallo diventare un array di reverse window istanziato nell'init

	tid = thread_id;
	window = create_new_revwin(0);

	while (!stop && !sim_error) {

		/*FETCH*/
		if (queue_min() == 0) {
			execution_time(INFTY);
			continue;
		}

		current_lp = current_msg.receiver_id;	//lp
		current_lvt = current_msg.timestamp;	//local virtual time

		while (1) {

///ESECUZIONE SAFE:
///non ci sono problemi quindi eseguo normalmente*/
			if ((pending_events = check_safety(current_lvt)) == -1) {
				get_lp_lock(0, 1);
				ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
				committed_safe[tid]++;
				release_lp_lock();

			}
///ESECUZNE HTM:
///non sono safe quindi ricorro ad eseguire eventi in htm*/
			else if (pending_events < reverse_execution_threshold) {	//questo controllo � da migliorare

				get_lp_lock(0, 1);

				if ((status = _xbegin()) == _XBEGIN_STARTED) {

					ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

					throttling(pending_events);
					
					if (check_safety(current_lvt) == 0) {
						_xend();
						committed_htm[tid]++;
						release_lp_lock();
					} else {
						_xabort(_ROLLBACK_CODE);
					}
				} else {	//se il commit della transazione fallisce, finisce qui
					status = _XABORT_CODE(status);
					if (status == _ROLLBACK_CODE)
						abort_conflict[tid]++;
					else
						abort_unsafety[tid]++;
					release_lp_lock();
					continue;
				}

			}
///ESECUZIONE REVERSIBILE:
///mi sono allontanato molto dal GVT, quindi preferisco un esecuzione reversibile*/
			else {
				if(get_lp_lock(1, 0)==0)
					continue; //Se non riesco a prendere il lock riparto da capo perche magari a questo giro rientro in modalit� transazionale

				reset_window(window);	//<-da mettere una volta sola ad inizio esecuzione
				ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

				continua = false;

				while (check_safety(current_lvt) > 0) {
					if (wait_time[current_lp] < current_lvt || (wait_time[current_lp] == current_lvt && tid > wait_time_id[current_lp])) {
						execute_undo_event(window);
						queue_clean();
						abort_waiting[tid]++;
						continua = true;
						break;
					}
				}
				release_lp_lock();
#ifdef FINE_GRAIN_DEBUG
				if (!continua)
					committed_reverse[tid]++;
#endif
				if (continua)
					continue;
			}

			break;
		}
		 /*FLUSH*/ 
		 flush();

		//Libero la memoria allocata per i dati dell'evento
		//    free(current_msg.data);

		if ((can_stop[current_lp] = OnGVT(current_lp, states[current_lp]))) //va bene cosi?
			stop = check_termination();

#ifdef THROTTLING
		if ((evt_count - HILL_CLIMB_EVALUATE * (evt_count / HILL_CLIMB_EVALUATE)) == 0)
			hill_climbing();
#endif

		//if(tid == _MAIN_PROCESS) {
		evt_count++;
		if ((evt_count - 100 * (evt_count / 100)) == 0) {	//10000
			printf("[%u] TIME: %f", tid, current_lvt);
			printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", committed_safe[tid], committed_htm[tid], committed_reverse[tid]);
		}
		//}

	}

	execution_time(INFTY);
	
	if(sim_error){
		printf("\n[%u] Execution ended for an error\n\n", tid);
	}else if (stop){
		printf("\n[%u] Execution ended correctly\n\n", tid);
	}

/*
	printf("Thread %d aborted %u times for cross check condition, %u for memory conflicts, and %u times for waiting thread\n" 
	"Thread %d executed in non-transactional block: %u\n" 
	"Thread %d executed in transactional block: %u\n"
	"Thread %d executed in reversible block: %u\n", 
	tid, abort_conflict[tid], abort_unsafety[tid], abort_waiting[tid], 
	tid, committed_safe[tid], 
	tid, committed_htm[tid], 
	tid, committed_reverse[tid]);
*/

	if(tid==0) print_report();
}
