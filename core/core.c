#define _GNU_SOURCE

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
#include <powercap.h>


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
				;
#endif

#if POWERCAP == 1
#include <time.h>
#define MILLION 1000000
#define BILLION 1000000000
#define EVENTS_BEFORE_STATS_PERIOD_NS (5)
#define STATS_PERIOD_NS ((POWERCAP_OBSERVATION_PERIOD/POWERCAP_WINDOW_SIZE)*MILLION)
#define PCAP_TOLLERANCE (0.10)

__thread double window_adv[POWERCAP_WINDOW_SIZE];
__thread double window_com[POWERCAP_WINDOW_SIZE];
__thread double window_tot[POWERCAP_WINDOW_SIZE];
__thread double window_rol[POWERCAP_WINDOW_SIZE];
__thread double window_dur[POWERCAP_WINDOW_SIZE];
__thread int window_phase = 0;
__thread struct timespec time_interval_for_stats_start;
__thread struct timespec time_interval_for_stats_end;
__thread double last_total_events = 0;
__thread double last_committed_events = 0;
__thread double last_advanced_events = 0;
__thread double last_rollback_events = 0;
__thread double previous_total_events = 0;
__thread double previous_committed_events = 0;
__thread double previous_advanced_events = 0;
__thread double previous_rollback_events = 0;

/*
 * TODO: aggiungere le modalità di selezione del valore:
 * 0-Classic: lavora sempre sulla media
 * 1-Max: selezione sempre e solo il massimo
 * 2-Max-filtrato: se non c'è troppa oscillazione, prendo il massimo
 * 3-Attuale: Calcolo la mediana e scarto tutto quello che in basso ne è fuori del 10%. Butto se il max è fuori del 10%
*/

static void powercap_gathering_stats(){
	double tot=0, adv=0, com=0, rol=0, total_events=0, committed_events=0, advanced_events=0, rollback_events=0;
	double median, max_from_median, min_from_median, sum_adv=0, sum_dur=0, sum_obs=0, sum_rol=0;
	unsigned int i;
	long duration;
	bool skip_heuristic = 0;

	sample_average_powercap_violation();

	//Check if is elapsed enough time from the last collection
	clock_gettime(CLOCK_MONOTONIC, &time_interval_for_stats_end);
	duration = time_interval_for_stats_end.tv_nsec - time_interval_for_stats_start.tv_nsec +
	                    BILLION * (time_interval_for_stats_end.tv_sec - time_interval_for_stats_start.tv_sec);

	if(duration > STATS_PERIOD_NS){
	    //Gather stats
		for(i = 0; i < n_prc_tot; i++) {
			total_events += lp_stats[i].events_total;
			advanced_events += LPS[i]->num_executed_frames;
			rollback_events += lp_stats[i].counter_rollbacks;
		}
		for(i = 0 ; i < n_cores; i++){
			committed_events += thread_stats[i].events_committed;
		}

        window_tot[window_phase % POWERCAP_WINDOW_SIZE] = total_events - last_total_events;
        window_adv[window_phase % POWERCAP_WINDOW_SIZE] = committed_events - last_committed_events;
        window_com[window_phase % POWERCAP_WINDOW_SIZE] = advanced_events - last_advanced_events;
        window_rol[window_phase % POWERCAP_WINDOW_SIZE] = rollback_events - last_rollback_events;
        window_dur[window_phase % POWERCAP_WINDOW_SIZE] = duration;
        window_phase++;

#if DEBUG_SMALL_SAMPLES == 1
        printf("RTA: %f\n",window_adv[(window_phase-1) % POWERCAP_WINDOW_SIZE]);
#endif

        //Task to be executed at the end of the window
        if(window_phase % POWERCAP_WINDOW_SIZE == 0) {
#if POWERCAP_SAMPLE_FILTERING == 2 || POWERCAP_SAMPLE_FILTERING == 3
            median = quick_select_median(window_adv, POWERCAP_WINDOW_SIZE);
            max_from_median = median * (1 + PCAP_TOLLERANCE);
            min_from_median = median * (1 - PCAP_TOLLERANCE);
#endif

#if DEBUG_SMALL_SAMPLES == 1
            //TODO: if we shift from time to evt, we have to consider the time passed as reference
            printf("\n---------------------\n");
#endif

            for(i = 0; i < POWERCAP_WINDOW_SIZE; i++){
#if DEBUG_SMALL_SAMPLES == 1
                printf("ADV: %f",window_adv[i]);
#endif

#if POWERCAP_SAMPLE_FILTERING == 0
                sum_adv += window_adv[i];
                sum_dur += window_dur[i];
#endif

//Get max value
#if POWERCAP_SAMPLE_FILTERING == 1 || POWERCAP_SAMPLE_FILTERING == 2
                if(window_adv[i] > sum_adv){
                    sum_adv = window_adv[i];
                    sum_dur = window_dur[i];
                }
#endif

//Decide if the current observation period has to be discarded
#if POWERCAP_SAMPLE_FILTERING == 2 || POWERCAP_SAMPLE_FILTERING == 3

                //If the median is far away from the max, this means that this run is corrupted
                if(window_adv[i] > max_from_median){
                    skip_heuristic = true;
                    #if DEBUG_SMALL_SAMPLES == 1
                        printf(" *  KILL RUN");
                    #endif
                }
#endif

//Collect vlues greater than median
#if POWERCAP_SAMPLE_FILTERING == 3
                //The results too low respect the median cannot be considered because affected by some housekeeping task
                if(window_adv[i] > min_from_median){
                    sum_adv += window_adv[i];
                    sum_dur += window_dur[i];
                    sum_rol += window_rol[i];
                    sum_obs++;
                    #if DEBUG_SMALL_SAMPLES == 1
                        if(window_adv[i] == median)
                            printf(" <> MEDIAN");
                    #endif
                } 
                #if DEBUG_SMALL_SAMPLES == 1
                else {
                    printf(" -  REMOVED");
                }
                #endif
#endif
#if DEBUG_SMALL_SAMPLES == 1
                printf("\n");
#endif
            }

            start_heuristic(sum_adv*BILLION/sum_dur, !skip_heuristic);

#if DEBUG_SMALL_SAMPLES == 1
            adv = com = tot = duration = 0;
            for (i = 0; i < POWERCAP_WINDOW_SIZE; i++) {
                adv += window_adv[i];
                com += window_com[i];
                tot += window_tot[i];
                rol += window_rol[i];
                duration += window_dur[i];
            }

            if (skip_heuristic == 1)
                printf("\033[0;31m");
            else if ((double)sum_obs/(double)POWERCAP_WINDOW_SIZE < 0.8)
                printf("\033[0;33m");
            printf("Len(ms):%f Adv:%f Com:%f Diff:%f Tot:%f Eff1:%f Eff2:%f Roll:%f Obs:%f Tot_obs:%d\n", 
            	duration/((double)MILLION), adv, com, adv-com, tot, com/tot, adv/tot, rol, sum_obs, POWERCAP_WINDOW_SIZE);
            printf("\033[0m");
/*
            previous_advanced_events = adv;
            previous_committed_events = com;
            previous_rollback_events = rol;
            previous_total_events = tot;
*/
#endif
        }

		//Update fields required for the next computation
		time_interval_for_stats_start.tv_nsec = time_interval_for_stats_end.tv_nsec;
		time_interval_for_stats_start.tv_sec = time_interval_for_stats_end.tv_sec;
		last_total_events = total_events;
		last_committed_events = committed_events;
		last_advanced_events = advanced_events;
		last_rollback_events = rollback_events;

	}
}
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
volatile bool stop = false;
volatile bool stop_timer = false;
pthread_t sleeper;//pthread_t p_tid[number_of_threads];//
unsigned int sec_stop = 0;
unsigned long long *sim_ended;


size_t node_size_msg_t;
size_t node_size_state_t;

void * do_sleep(){
#if SUPPRESS_USE_PRINTS == 0
	printf("The process will last %d seconds\n", sec_stop);
#endif	

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
	id_cpu = tid; // (tid % 8) * 4 + (tid/((unsigned int)8));
	printf("Thread %u set to CPU no %u\n", tid, id_cpu);
	CPU_ZERO(&mask);
	CPU_SET(id_cpu, &mask);
    pthread_t current_thread = pthread_self();
    int err = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &mask);
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
		LPS[i]->epoch 					= 1;
		LPS[i]->num_executed_frames		= 0;
		LPS[i]->until_clean_ckp			= 0;

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
///////////			 _	 _	 _	 _	      _   __   _   __		/////////////////////////////////////
///////////			| \	| |	| \	/ \      / \ |  | / \ |  |		/////////////////////////////////////
///////////			|_/	|_|	| | '-.  __   _/ |  |  _/ |  |		/////////////////////////////////////
///////////			|	| |	|_/	\_/      /__ |__| /__ |__|		/////////////////////////////////////
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

#if POWERCAP == 1
    //init the power subsystem for each thread
    init_powercap_thread(tid);
#endif

#if REVERSIBLE == 1
	reverse_init(REVWIN_SIZE);
#endif

	// Set the CPU affinity
	set_affinity(tid);//TODO: decommentare per test veri
	
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



	
	if(tid == 0){
		LPs_metada_init();
		dymelor_init();
		statistics_init();
		queue_init();
		numerical_init();
		nodes_init();
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
			((state_t*)((rootsim_list*)LPS[current_lp]->queue_states)->head->data)->lvt = -1;// Serve per tirare fuori l'INIT dalla timeline
			LPS[current_lp]->state_log_forced = false;
			LPS[current_lp]->until_ongvt = 0;
			LPS[current_lp]->until_ckpt_recalc = 0;
			LPS[current_lp]->ckpt_period = 20;
			//LPS[current_lp]->epoch = 1;
		}

		nbcalqueue->hashtable->current  &= 0xffffffff;//MASK_EPOCH

#if SUPPRESS_USE_PRINTS == 0		
		printf("EXECUTED ALL INIT EVENTS\n");
#endif
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

		ProcessEvent(LP, event_ts, event_type, event_data, event_data_size, lp_state);

#if REPORT == 1
		execute_time = clock_timer_value(event_processing_timer);

		statistics_post_lp_data(LP, STAT_EVENT, 1);
		statistics_post_lp_data(LP, STAT_CLOCK_EVENT, execute_time);

		if(LPS[current_lp]->state == LP_STATE_SILENT_EXEC){
			statistics_post_lp_data(LP, STAT_CLOCK_SILENT, execute_time);
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


void thread_loop(unsigned int thread_id) {
	unsigned int old_state;
	
#if ONGVT_PERIOD != -1
	unsigned int empty_fetch = 0;
#endif
	
	init_simulation(thread_id);
	
#if REPORT == 1 
	clock_timer_start(main_loop_time);
#endif

#if POWERCAP == 1
	clock_gettime(CLOCK_MONOTONIC, &time_interval_for_stats_start);
#endif

	///* START SIMULATION *///
	while (  
				(
					 (sec_stop == 0 && !stop) || (sec_stop != 0 && !stop_timer)
				) 
				&& !sim_error
		) {

#if POWERCAP == 1
	//The main thread, the one with tid=0, is used to control other threads
	//This means that the main thread is the one that cannot go to sleep
	if (tid == 0){
	    if (evt_count%EVENTS_BEFORE_STATS_PERIOD_NS == 0)
		    powercap_gathering_stats();
    }
	//The other thread can be put to sleep at this point of the execution since here they have nothing to be executed
	else{
        check_running_array(tid);
	}
#endif

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
			current_msg->roll_epoch = LPS[current_lp]->epoch;
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

		if(current_evt_state == ANTI_MSG) {
			current_msg->monitor = (void*) 0xba4a4a;

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
		queue_deliver_msgs();

#if DEBUG == 1
		if((unsigned long long)current_msg->node & 0x1){
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
#if SUPPRESS_USE_PRINTS == 0
		printf(RED("[%u] Execution ended for an error\n"), tid);
#endif	
	} else if (stop || stop_timer){
		//sleep(5);
#if SUPPRESS_USE_PRINTS == 0
		printf(GREEN( "[%u] Execution ended correctly\n"), tid);
#endif
		if(tid==0){
			pthread_join(sleeper, NULL);
		}
	}
}
