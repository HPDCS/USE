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
#define DELTA 500  // tick count
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

bool abbiamounproblema = false; //da cancellare

unsigned int reverse_execution_threshold = 2; //ho messo un valore a caso, ma sarà da fissare durante l'inizializzazione


#define FINE_GRAIN_DEBUG


void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);\
	fflush(stderr);

	if(fatal) {
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
	if(opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// opath plus 1 is a hack to allow also absolute path
	for(p = opath + 1; *p; p++) {
		if(*p == '/') {
			*p = '\0';
			if(access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if(access(opath, F_OK)) {
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

  if(delta_count == 0)
	return;
    //for(i = 0; i < 1000; i++);

  tick_count = CLOCK_READ();
  while(true) {
	if(CLOCK_READ() > tick_count + 	events * DELTA * delta_count)
		break;
  }
}

void hill_climbing(void) {
	if((double)abort_count_safety / (double)evt_count < abort_percent && delta_count < HIGHEST_COUNT) {
		delta_count++;
		//printf("Incrementing delta_count to %d\n", delta_count);
	} else {
/*		if(random() / RAND_MAX < HILL_EPSILON_GREEDY) {
			delta_count /= (random() / RAND_MAX) * 10 + 1;
		}
*/	}

	abort_percent = (double)abort_count_safety / (double)evt_count;
}


void SetState(void *ptr) {
	states[current_lp] = ptr;
}

static void process_init_event(void) {
    unsigned int i;

    ///ma questo lo fa tutto il processo zero?
    for(i = 0; i < n_prc_tot; i++) {
        current_lp = i;
        current_lvt = 0;
        ProcessEvent(current_lp, current_lvt, INIT, NULL, 0, states[current_lp]);
        queue_deliver_msgs();
    }
}

void init(unsigned int _thread_num, unsigned int lps_num){
    unsigned int i;

    printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
    n_cores = _thread_num;
    n_prc_tot = lps_num;
    states = malloc(sizeof(void *) * n_prc_tot);
    can_stop = malloc(sizeof(bool) * n_prc_tot);

/*MAURO*/
    lp_lock = malloc(sizeof(int) * lps_num);    //mauro: inizializzazione array. Questo dovrebbe bastare a portare tutto a 0
    wait_time = malloc(sizeof(simtime_t) * lps_num);    //mauro: inizializzazione array. Questo dovrebbe bastare a portare tutto a 0
    wait_time_id = malloc(sizeof(unsigned int) * lps_num);
    wait_time_lk = malloc(sizeof(int) * lps_num);
    for(i = 0; i < lps_num; i++){
        lp_lock[i] = 0;
        wait_time_id [i] =_thread_num+1;
        wait_time[i] = INFTY;
        wait_time_lk[i] = 0;
    }

/*MAURO*/
    #ifndef NO_DYMELOR
    dymelor_init();
    #endif
    queue_init();
    message_state_init();
    numerical_init();

    //  queue_register_thread();
    process_init_event();
}


bool check_termination(void) {
	int i;
	bool ret = true;
	for(i = 0; i < n_prc_tot; i++) {
		ret &= can_stop[i];
	}
	return ret;
}

void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size){
    if(timestamp<current_lvt){ //da cancellare
            printf("Ma allora me stai prendendo in giro!!!!\n");
            abort();
    }
    queue_insert(receiver, timestamp, event_type, event_content, event_size);
}

/**
* @author Mauro Ianni
*/

double double_cas(double * addr, double old_val, double new_val){
    long long res;

    res = __sync_val_compare_and_swap(UNION_CAST(addr, long long *), UNION_CAST(old_val, long long), UNION_CAST(new_val, long long));

    return UNION_CAST(res, double);
}

/**
* @author Mauro Ianni
*/
int get_lp_lock(unsigned int mode, unsigned int bloc){
//mode  0=condiviso      1=esclusivo
//bloc  0=non bloccante  1=bloccante
    int old_lk;
    simtime_t old_tm;
    unsigned int old_tm_id;
    //cosi facendo il controllo sul "mode" viene fatto una volta sola

    //esclusivo
    if(mode){
        do{
            while(wait_time[current_lp]<current_lvt || (wait_time[current_lp]==current_lvt && tid>wait_time_id[current_lp])); //aspetto di diventare il min

            if( (old_lk = lp_lock[current_lp]) == 0){
                if( __sync_val_compare_and_swap(&lp_lock[current_lp], 0, -1)==0 ){
                    return 1;
               }
            }
            else{ //voglio prendere il lock esclusivo ma non posso perche non è libero
                while(1){   //PER VEDERE VECCHIA IMPLEMENTAZIONE, ANDARE ALLE VERSIONI PRECEDENTE AL 19 LUGLIO
                    while(__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
                        while(wait_time_lk[current_lp]);

                    old_tm_id= wait_time_id[current_lp];
                    old_tm = wait_time[current_lp]; //vedo su quell'lp

                    if(tid==old_tm_id){
                        __sync_lock_release(&wait_time_lk[current_lp]);
                        break;
                    }
                    else if(current_lvt < old_tm || (current_lvt == old_tm && tid<old_tm_id)){
                        wait_time[current_lp] = current_lvt;
                        wait_time_id[current_lp] = tid;
                        __sync_lock_release(&wait_time_lk[current_lp]);
                        break;
                    }
                    __sync_lock_release(&wait_time_lk[current_lp]);

                }
            }
        //qui forse devo farlo temporeggiare
        }while(bloc);
    }

    //condiviso
    else{
        do{
            if( (old_lk = lp_lock[current_lp]) >= 0){
                if(__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk+1)==old_lk){
                    //printf("Io processo %u ho preso il lock condiviso al tempo %f",tid, current_lvt);
                    return 1;
                }

            }
             else{ //voglio prendere il lock esclusivo ma non posso perche non è libero
                while(1){
                    while(__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
                        while(wait_time_lk[current_lp]);

                    old_tm_id= wait_time_id[current_lp];
                    old_tm = wait_time[current_lp]; //vedo su quell'lp

                    if(tid==old_tm_id){
                        __sync_lock_release(&wait_time_lk[current_lp]);
                        break;
                    }

                    else if(current_lvt < old_tm || (current_lvt == old_tm && tid<old_tm_id)){
                        wait_time[current_lp] = current_lvt;
                        wait_time_id[current_lp] = tid;
                        __sync_lock_release(&wait_time_lk[current_lp]);
                        break;
                    }
                    __sync_lock_release(&wait_time_lk[current_lp]);

                }
            }
        //qui forse devo farlo temporeggiare
        }while(bloc);
    }

    return 0; //arriva qui solo se il lock è non bloccante e non ha potuto prenderlo

}

/**
* @author Mauro Ianni
*/
void release_lp_lock(){

    int old_lk;

    if(wait_time_id[current_lp]==tid){ //forse posso fare direttamente i cas
        while(__sync_lock_test_and_set(&wait_time_lk[current_lp], 1))
            while(wait_time_lk[current_lp]);

        if(wait_time_id[current_lp]==tid){
            wait_time_id[current_lp]=n_cores+1;
            wait_time[current_lp]=INFTY;
        }
        __sync_lock_release(&wait_time_lk[current_lp]);

    }
    old_lk = lp_lock[current_lp];

    //metto questo fuori dal ciclo per evitarmi ogni volta il controllo
    if(old_lk == -1){
         __sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, 0); ///posso anche non fare il cas perche tanto solo uno alla volta rilascia il lock esclusivo
    }
    else{
         do{
            if(old_lk > 0){///<-non serve questo controllo perche se mi trovo qui sara sempre maggiore di zero
                if(__sync_val_compare_and_swap(&lp_lock[current_lp], old_lk, old_lk-1)==old_lk){
                    return;
                }
            }
            old_lk =lp_lock[current_lp]; //mettendono alla fine risparmio una lettura
         }while(1);
    }

}


void thread_loop(unsigned int thread_id){

    int status;
    unsigned int pending_events;

    bool continua;

    revwin *window;

    #ifdef FINE_GRAIN_DEBUG
    unsigned int non_transactional_ex = 0, transactional_ex = 0, reversible_ex = 0;
    #endif

    tid = thread_id;
    int cont = 0;// da cancellare
    
    queue_clean();

    while(!stop && !sim_error){

        /*FETCH*/
        if(queue_min() == 0){ //while(fetch()==0);
			execution_time(INFTY);
            continue;
        }

        current_lp = current_msg.receiver_id;   //lp
        current_lvt  = current_msg.timestamp;   //local virtual time

        cont = 0; //da cancellare


        while(1){
/*
                if(current_lvt>105){
                    printf("W: il processo con id %u e tempo %f, è bloccato nel loop, con %u eventi avanti.\n", tid, current_lvt, check_safety());
                    for(int k; k < n_cores; k++){
                        printf("W: porcessing[%d] =%e, in_transit[%d]=%e\n", k,get_processing(k),k, get_intransit(k));
                    }
                }
/*
                cont++;//da cancellare
                if(0 &&cont!=0 && cont%1000==0){
                    printf("A: il processo con id %u e tempo %f, è bloccato nel loop, con %u eventi avanti\n", tid, current_lvt, check_safety());
                    abbiamounproblema = true;
                    printf("A: il processo con id %u e tempo %f, è bloccato nel loop, con %u eventi avanti\n", tid, current_lvt, check_safety());
                    for(int i; i < n_cores; i++){
                        printf("A: porcessing[%d] =%f, in_transit[%d]=%f\n", i,get_processing(i),i, get_intransit(i));
                    }
                    fflush(stdout);
                }
                if(0&&abbiamounproblema && cont<100){
                    printf("B: il processo con id %u e tempo %f, è abbiamounproblema\n", tid, current_lvt);
                    abbiamounproblema=false;
                    printf("B: il processo con id %u e tempo %f, è bloccato nel loop, con %u eventi avanti\n", tid, current_lvt, check_safety());
                    for(int i; i < n_cores; i++){
                        printf("B: porcessing[%d] =%f, in_transit[%d]=%f\n", i,get_processing(i),i, get_intransit(i));
                    }
                }
*/
            //printf("pedn_evt: %u", pending_events);

///ESECUZIONE SAFE:
///non ci sono problemi quindi eseguo normalmente*/
            //if(check_safety_old(&pending_events)){//
            if((pending_events=check_safety())==0){
                for(int i; i < n_cores; i++) if(current_lvt > get_processing(i) || current_lvt> get_intransit(i)){
                    printf("======MA MICA PO ESSE CHE STO QUI====\n");
                    printf("======il processo con id %u e tempo %f, è bloccato nel loop, con %u eventi avanti\n", tid, current_lvt, check_safety());
                    for(int i; i < n_cores; i++){
                    printf("======porcessing[%d] =%f, in_transit[%d]=%e\n", i,get_processing(i),i, get_intransit(i));
                    }printf("\n");
                    fflush(stdout);
                }
                get_lp_lock(0,1);

                ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
                #ifdef FINE_GRAIN_DEBUG
                __sync_fetch_and_add(&non_transactional_ex, 1);
                #endif
                release_lp_lock();

            }
///ESECUZNE HTM:
///non sono safe quindi ricorro ad eseguire eventi in htm*/
            else if (pending_events < reverse_execution_threshold){//questo controllo è da migliorare

                get_lp_lock(0,1);

                if( (status = _xbegin()) == _XBEGIN_STARTED){

                    ProcessEvent(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);

                    throttling(pending_events);
                    //if(check_safety_old(&pending_events)){//
                    if(check_safety()==0){
                        _xend();
                        #ifdef FINE_GRAIN_DEBUG
                        __sync_fetch_and_add(&transactional_ex, 1);
                        #endif
                        release_lp_lock();
                    }
                    else{
                        _xabort(_ROLLBACK_CODE);
                    }
                }
                else{ //se il commit della transazione fallisce, finisce qui
                    status = _XABORT_CODE(status);
                    if(status == _ROLLBACK_CODE)
                        abort_count_conflict++;
                    else
                        abort_count_safety++;
                    release_lp_lock();
                    continue;
                }

            }
///ESECUZIONE REVERSIBILE:
///mi sono allontanato molto dal GVT, quindi preferisco un esecuzione reversibile*/
            else{
                    printf("CHE CAZZO CI FACCIO QUI!!!\n");
                get_lp_lock(1,1);

                window = create_new_revwin(0); //<-da mettere una volta sola ad inizio esecuzione
                ProcessEvent_reverse(current_lp, current_lvt, current_msg.type, current_msg.data, current_msg.data_size, states[current_lp]);
                finalize_revwin();//<- va tolto con il nuovo reverse perche lo fa da solo

                //qui c'era del throttling ma credo abbia poco senso

                continua = false;

                while(check_safety()!=0) {
                //while(!check_safety_old(&pending_events)) {//
                    if(wait_time[current_lp]<current_lvt || (wait_time[current_lp]==current_lvt && tid>wait_time_id[current_lp]) ){
                        execute_undo_event(window);
                        queue_clean();
                        abort_count_reverse++;
                        continua=true;
                        break;
                    }
                }
                release_lp_lock();
                #ifdef FINE_GRAIN_DEBUG
                if(!continua)__sync_fetch_and_add(&reversible_ex, 1);
                #endif
                free_revwin(window); //<-da spostare a fine esecuzione

                if(continua) continue;
            }


            break;
        }
        /*FLUSH*/
        flush();


/*    if(queue_pending_message_size())
      min_output_time(queue_pre_min());
    commit_time();
    queue_deliver_msgs();
  */
    //Libero la memoria allocata per i dati dell'evento
//    free(current_msg.data);

        can_stop[current_lp] = OnGVT(current_lp, states[current_lp]);
        stop = check_termination();

        #ifdef THROTTLING
        if((evt_count - HILL_CLIMB_EVALUATE * (evt_count / HILL_CLIMB_EVALUATE)) == 0)
            hill_climbing();
        #endif

        //if(tid == _MAIN_PROCESS) {
            evt_count++;
            if((evt_count - 10000 * (evt_count / 10000)) == 0){ //10000
                printf("[%u] TIME: %f",tid, current_lvt);
                printf(" \tsafety=%d \ttransactional=%d \treversible=%d\n", non_transactional_ex, transactional_ex, reversible_ex);
            }
        //}

        //printf("Timestamp %f executed\n", evt.timestamp);
    }
    
    execution_time(INFTY);

    #ifndef FINE_GRAIN_DEBUG
    printf("Thread %d aborted %llu times for cross check condition and %llu for memory conflicts\n", tid, abort_count_conflict, abort_count_safety);
    #endif

    #ifdef FINE_GRAIN_DEBUG
        printf("Thread %d aborted %llu times for cross check condition, %llu for memory conflicts, and %llu times for waiting thread\n"
               "Thread %d executed in non-transactional block: %d\n"
                "Thread %d executed in transactional block: %d\n",
                tid, abort_count_conflict, abort_count_safety, abort_count_reverse,
                tid, non_transactional_ex,
                tid, transactional_ex);
    #endif

}















