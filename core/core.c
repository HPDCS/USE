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

double delta_count = 0.3;

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

//Soglia dopo la quale gli eventi vengono eseguiti in modalità reversibile
unsigned int reverse_execution_threshold = 2;	//ho messo un valore a caso, ma sarà da fissare durante l'inizializzazione

#define FINE_GRAIN_DEBUG

//forse da cancellare
unsigned int * committed_safe;
unsigned int * committed_htm;
unsigned int * committed_reverse;
unsigned int * abort_unsafety;
unsigned int * abort_conflict;
unsigned int * abort_waiting;
unsigned int * abort_cachefull;
unsigned int * abort_debug;
unsigned int * abort_generic;
unsigned int * abort_nested;

//Durata media di un evento in cicli di clock
__thread unsigned long long event_clocks; //Questo poteva essere un valore globale, ma in questo modo ci evitiamo l'aggiornamento atomico, con prestazioni comunque simili

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
	
	tick_count = CLOCK_READ() + events * (event_clocks * delta_count); 
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
	
	event_clocks = 0;

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
	abort_cachefull = malloc(sizeof(unsigned int) * n_cores);
	abort_debug = malloc(sizeof(unsigned int) * n_cores);
	abort_generic = malloc(sizeof(unsigned int) * n_cores);
	abort_nested = malloc(sizeof(unsigned int) * n_cores);
		
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
		abort_cachefull[i] = 0;
		abort_debug[i] = 0;
		abort_generic[i] = 0;
		abort_nested[i] = 0;
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
	unsigned int tot_abort_cahcefull = 0;
	unsigned int tot_abort_debug = 0;
	unsigned int tot_abort_generic = 0;
	unsigned int tot_abort_nested = 0;
			
	printf("\n\n|\tTID\t||\tSafe\t|\tHtm\t|\tRevers\t||\tTOTAL\t|\n");
	printf("|---------------||--------------|---------------|---------------||--------------|\n");
	for(i = 0; i < n_cores; i++){
		printf("|\t[%u]\t||\t%u\t|\t%u\t|\t%u\t||\t%u\t|\n", i, committed_safe[i], committed_htm[i], committed_reverse[i], (committed_safe[i]+committed_htm[i]+committed_reverse[i]) );
		tot_committed_safe += committed_safe[i];
		tot_committed_htm += committed_htm[i];
		tot_committed_reverse += committed_reverse[i];
	}
	printf("|---------------||--------------|---------------|---------------||--------------|\n");
	printf("|\tTOT\t||\t%u\t|\t%u\t|\t%u\t||\t%u\t|\n", tot_committed_safe, tot_committed_htm, tot_committed_reverse, (tot_committed_safe+tot_committed_htm+tot_committed_reverse) );
	
	printf("\n\n|\tTID\t||\tUnsafe\t|\tCacFull\t|\tDebug\t|\tConfli\t|\tNested\t|\tGenric\t||\tWait\t|   |\tTOTAL\t|\n");
	printf("|---------------||--------------|---------------|---------------|---------------|---------------|---------------||--------------|   |-----------|\n");
	for(i = 0; i < n_cores; i++){
		printf("|\t[%u]\t||\t%u\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t||\t%u\t|   |\t%u\t|\n", i, abort_unsafety[i], abort_cachefull[i], abort_debug[i], abort_conflict[i],abort_nested[i], abort_generic[i], abort_waiting[i], (abort_unsafety[i]+abort_conflict[i]+abort_waiting[i]) );
		tot_abort_unsafety += abort_unsafety[i];
		tot_abort_conflict += abort_conflict[i];
		tot_abort_waiting += abort_waiting[i];
		tot_abort_cahcefull += abort_cachefull[i];
		tot_abort_debug += abort_debug[i];
		tot_abort_nested += abort_nested[i];
		tot_abort_generic += abort_generic[i];
	}
	printf("|---------------||--------------|---------------|---------------|---------------|---------------|---------------||--------------|   |-----------|\n");
	printf("|\tTOT\t||\t%u\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t|\t%u\t||\t%u\t|   |\t%u\t|\n\n", tot_abort_unsafety, tot_abort_cahcefull, tot_abort_debug, tot_abort_conflict, tot_abort_nested, tot_abort_generic, tot_abort_waiting, (tot_abort_unsafety+tot_abort_conflict+tot_abort_waiting) );

#ifdef THROTTLING
	printf("\n\n%u\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t%f\n", tot_committed_safe, tot_committed_htm,tot_abort_unsafety,tot_abort_cahcefull,tot_abort_conflict,tot_abort_generic, delta_count);
#else
	printf("\n\n%u\t%u\t\t%u\t\t%u\t\t%u\t\t%u\n", tot_committed_safe, tot_committed_htm,tot_abort_unsafety,tot_abort_cahcefull,tot_abort_conflict,tot_abort_generic);
#endif
}



//per ora non viene usato:
//L'idea è di mettere un unico thread con lo scopo di regolare le variabili

void *tuning(void *args){
	unsigned int committed, old_committed, throughput, old_throughput, last_op, field, i;
	old_throughput = 0;
	last_op = 1;
	//field = 1; // 0 sta per delta_count ed 1 sta per reverse_execution_threshold 
	double delta = 0.1;
	//unsigned int change_direction = 0;
	//unsigned int right_direction = 0;
	
	while(!stop && !sim_error){
		sleep(3);
		throughput = 0;
		committed = 0;
		
		for(i = 0; i <	n_cores; i++)
			committed += (committed_safe[i] + committed_htm[i] + committed_reverse[i]); //totale transazioni commitatte
		throughput = committed - old_committed;
	
		if(throughput < old_throughput)//poichè ho visto un peggioramento, inverto la direzione
			last_op=~last_op;	
			
		if(last_op==1) 
			delta_count+=delta;
		else 
			delta_count-=delta;
			
			
		if(delta_count<0) 
			delta_count=0;
		if(delta_count>1)
			delta_count=1;
				
		printf("Throughput %u : Delta : %f\n", throughput, delta_count);
		
		old_committed = committed;
		old_throughput = throughput;
	}


/*
	while(!stop && !sim_error){
		sleep(2);
		throughput = 0;
		committed = 0;
		
		for(i = 0; i <	n_cores; i++)
			committed += (committed_safe[i] + committed_htm[i] + committed_reverse[i]); //totale transazioni commitatte
		throughput = committed - old_committed;
		
		//gestisco eventuali cambi di direzione
		if(throughput > old_throughput){
			right_direction++;
			if(right_direction>1) 
				change_direction = 0;	
		}else{ //poichè ho visto un peggioramento, inverto la direzione
			last_op=~last_op; 
			change_direction++;
			right_direction = 0;	
		}
		
		
		//* UNDO THRESHOLD 
		if(field == 1){
			if(last_op==1) 
				reverse_execution_threshold++;
			else 
				reverse_execution_threshold--; 
			if(reverse_execution_threshold < 1){ 
				reverse_execution_threshold = 1;
				change_direction++;
				right_direction = 0;
			}
			else if(reverse_execution_threshold > n_cores){ 
				reverse_execution_threshold = n_cores;
				change_direction++;
				right_direction = 0;
			}
			printf("Throughput %u : Threshold : %u\n", throughput, reverse_execution_threshold);
		}
		
		//* DELTA THROTTLING 
		else{
			if(last_op==1) 
				delta_count+=delta;
			else 
				delta_count-=delta;
			if(delta_count<0){ 
				delta_count=0;
				//change_direction++;
				//right_direction = 0;
			}	
			printf("Throughput %u : Delta : %f\n", throughput, delta_count);
		}
		
		//gestisco stabilizzazione
		if(change_direction >= 3){
			printf("Mi sono stabilizzato\n");
			change_direction = 0;
			right_direction = 0;
			last_op=0; //dovrei fare in modo che sia coerente con quella vista prima
			field=~field;			
		}	
				
		old_committed = committed;
		old_throughput = throughput;
	}
	*/

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
	unsigned int status, pending_events;
		
	unsigned long long t_pre, t_post;// per throttling
	
	bool retry_event;

	revwin_t *window; //fallo diventare un array di reverse window istanziato nell'init

	// Initialize the SLAB allocator for this thread
	reverse_init(REVWIN_SIZE);
	
	tid = thread_id;
	window = revwin_create();

	while (!stop && !sim_error) {

		/*FETCH*/
		if (queue_min() == 0) {
			execution_time(INFTY,-1);
			continue;
		}

		//lvt ed lp dell'evento corrente
		current_lp = current_msg.receiver_id;	//identificatore lp
		current_lvt = current_msg.timestamp;	//local virtual time
		
		while (1) {
			

///ESECUZIONE SAFE:
///non ci sono problemi quindi eseguo normalmente*/
			if ((pending_events = check_safety(current_lvt)) == 0) {  //if ((pending_events = check_safety_lookahead(current_lvt)) == 0) {
				//printf("%u SAF \ttime:%f \tlp:%u\n",tid, current_lvt, current_lp);
#ifdef REVERSIBLE
				get_lp_lock(0, 1);
#endif
				t_pre = CLOCK_READ();// per throttling
				ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
				t_post = CLOCK_READ();// per throttling
#ifdef REVERSIBLE
				release_lp_lock();
#endif
				committed_safe[tid]++;
#ifdef THROTTLING
				//guarda se si può migliorare
				if(event_clocks > 0)
					event_clocks = (event_clocks*0.9) + ((t_post-t_pre)*0.1);// per throttling
				else 
					event_clocks = (t_post-t_pre);// per throttling
#endif				

			}
///ESECUZNE HTM:
///non sono safe quindi ricorro ad eseguire eventi in htm*/
			else if (pending_events < reverse_execution_threshold) {
				//printf("%u HTM \ttime:%f \tlp:%u\n",tid, current_lvt, current_lp);
#ifdef REVERSIBLE
				get_lp_lock(0, 1);
#endif
				if ((status = _xbegin()) == _XBEGIN_STARTED) {

					ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
#ifdef THROTTLING
					throttling(pending_events);
#endif
					if (check_safety(current_lvt) == 0) {
						_xend();
						committed_htm[tid]++;
#ifdef REVERSIBLE
						release_lp_lock();
#endif
					} else {
						_xabort(_ROLLBACK_CODE);
					}
				} else {	//se il commit della transazione fallisce, finisce qui
					if (status & _XABORT_CAPACITY)
						abort_cachefull[tid]++;
					else if (status & _XABORT_DEBUG)
						abort_debug[tid]++;
					else if (status & _XABORT_CONFLICT)
						abort_conflict[tid]++;
					else if (_XABORT_CODE(status) == _ROLLBACK_CODE)
						abort_unsafety[tid]++;
					else{
						abort_generic[tid]++;
					}
#ifdef REVERSIBLE
					release_lp_lock();
					//goto reversible;
#endif
					continue;
				}

			}
///ESECUZIONE REVERSIBILE:
///mi sono allontanato molto dal GVT, quindi preferisco un esecuzione reversibile*/
#ifdef REVERSIBLE
			else {
reversible:			//printf("%u REV \ttime:%f \tlp:%u\n",tid, current_lvt, current_lp);
				if(get_lp_lock(1, 0)==0)
					continue; //Se non riesco a prendere il lock riparto da capo perche magari a questo giro rientro in modalità transazionale

				revwin_reset(current_lp, window);	//<-da mettere una volta sola ad inizio esecuzione
				//ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
				ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
				///ATTENZIONE, da rimettere quello reversibile
				retry_event = false;

				while (check_safety_lookahead(current_lvt) > 0) {
					if ( check_waiting() ) {
						execute_undo_event(current_lvt, window);
						queue_clean();
						abort_waiting[tid]++;
						retry_event = true;
						break;
					}
				}
				
				release_lp_lock();
				

				if (retry_event)
					continue;
				
				committed_reverse[tid]++;
				
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
				printf(" \tsafety=%u \ttransactional=%u \treversible=%u\n", committed_safe[tid], committed_htm[tid], committed_reverse[tid]);
			}
		}

	}

	execution_time(INFTY,-1);

	// Destroy SLAB's structures    
	//reverse_fini();
	
	if(sim_error){
		printf("\n[%u] Execution ended for an error\n\n", tid);
	}else if (stop){
		printf("\n[%u] Execution ended correctly\n\n", tid);
	}
}
