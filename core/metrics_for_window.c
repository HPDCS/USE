#if ENFORCE_LOCALITY == 1
#include <timer.h>
#include <core.h>
#include <pthread.h>
#include <stdbool.h>
#include <statistics.h>
#include <window.h>
#include <clock_constant.h>

window w;

static double old2_thr, old_thr, thr_ref;
static simtime_t prev_granularity, prev_granularity_ref, granularity_ref;
static unsigned int prev_comm_evts, prev_comm_evts_ref, prev_first_time_exec_evts, prev_comm_evts_window, prev_first_time_exec_evts_ref;


static pthread_mutex_t stat_mutex, window_check_mtx;


static __thread clock_timer start_window_reset;
static __thread stat64_t time_interval_for_measurement_phase;
static __thread double elapsed_time;

void init_metrics_for_window() {

	init_window(&w);
	pthread_mutex_init(&stat_mutex, NULL);
	pthread_mutex_init(&window_check_mtx, NULL);

	prev_comm_evts = 0;
	prev_comm_evts_ref = 0;
	prev_first_time_exec_evts = 0;
	prev_first_time_exec_evts_ref = 0;
	prev_comm_evts_window = 0;

	thr_ref = 0.0;
	granularity_ref = 0.0;
	prev_granularity_ref = 0.0;
	prev_granularity = 0.0;
	old_thr = 0.0;
}


int check_window(){
	#if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 1
	  if (!w.enabled && w.size == 0) clock_timer_start(start_window_reset);
	  return w.size > 0;
	#else
    return 1;
	#endif
}

simtime_t get_current_window(){
	#if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 1
  	return w.size;
	#else
  	return START_WINDOW;
  #endif
}

double compute_throughput_for_committed_events(unsigned int *prev_comm_evts, clock_timer timer) {

	unsigned int i;
	unsigned int comm_evts = 0;
	double th = 0.0;
	simtime_t time_interval = clock_timer_value(timer)/CLOCKS_PER_US;

	for (i = 0; i < n_cores; i++) {
		comm_evts += thread_stats[i].events_committed;
	}
	th += comm_evts - *prev_comm_evts;
	*prev_comm_evts = comm_evts;

	return (double) th/time_interval;

}

simtime_t compute_average_granularity(unsigned int *prev_first_time_exec_evts, simtime_t *prev_granularity) {

	unsigned int i;
	unsigned int first_time_exec_evts = 0;
	simtime_t granularity = 0.0;

	for (i = 0; i < n_prc_tot; i++)	{
		first_time_exec_evts += (lp_stats[i].events_total - lp_stats[i].events_silent);
		granularity += lp_stats[i].clock_event;
	}

	granularity -= *prev_granularity;
	first_time_exec_evts -= *prev_first_time_exec_evts;

	*prev_granularity = granularity; 
	granularity /= first_time_exec_evts;
	*prev_first_time_exec_evts = first_time_exec_evts;

	return granularity;

}


void enable_window() {

	double current_thr = 0.0;
	double diff1 = 0.0, diff2 = 0.0;
	current_thr = compute_throughput_for_committed_events(&prev_comm_evts ,w.measurement_phase_timer);

#if VERBOSE == 1
	printf("current_thr/old_thr %f\n", current_thr/old_thr);
#endif

	//check if window must be enabled
	//by comparing current thr with the previous one
	if(old2_thr != 0.0){
		diff1 = old2_thr/current_thr;
		diff2 = old_thr/current_thr;
		if(diff1 < 1.025 && diff2 < 1.025 && diff1 > 0.975 && diff2 > 0.975)  w.enabled = 1;
	}
	printf("stability check: %2.f%% %2.f%%\n", 100*diff1, 100*diff2);
	
	old2_thr = old_thr;
	old_thr = current_thr;

}



void aggregate_metrics_for_window_management(window *win) {
 #if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 0
   return;
 #endif
	double thr_window = 0.0, thr_ratio = 0.0;
	simtime_t granularity = 0.0, granularity_ratio = 0.0;
	time_interval_for_measurement_phase = clock_timer_value(w.measurement_phase_timer);
	elapsed_time = time_interval_for_measurement_phase / CLOCKS_PER_US;
	if (elapsed_time/1000.0 < MEASUREMENT_PHASE_THRESHOLD_MS) return;

	if (pthread_mutex_trylock(&stat_mutex) == 0) { //just one thread executes the underlying block

		if (!w.enabled) enable_window(&old_thr);
		
		//window is enabled -- workload can be considered stable
		if (w.enabled) {
			
			thr_window = compute_throughput_for_committed_events(&prev_comm_evts_window, w.measurement_phase_timer);
			granularity = compute_average_granularity(&prev_first_time_exec_evts, &prev_granularity);

			//fprintf(stderr, "SIZE BEFORE RESIZING %f\n", *window_size);
			window_resizing(win, thr_window); //do resizing operations -- window might be enlarged, decreased or stay the same

	#if VERBOSE == 1
			printf("thr_window %f granularity %f\n", thr_window, granularity);
			printf("window_resizing %f\n", *window_size);
        #endif	
			if(w.phase == 1){
                          thr_ref = thr_window;
                          granularity_ref = granularity;
                          w.phase++;
                        }
			//fprintf(stderr, "SIZE AFTER RESIZING %f\n", *window_size);
			//thr_ref = compute_throughput_for_committed_events(&prev_comm_evts_ref, start_window_reset);
			//granularity_ref = compute_average_granularity(&prev_first_time_exec_evts_ref, &prev_granularity_ref);
                        else if(w.phase >= 2){
           		  thr_ratio = thr_window/thr_ref;
                          granularity_ratio = granularity/granularity_ref;
    #if VERBOSE == 1
			  printf("thr_ref %f granularity_ref %f\n", thr_ref, granularity_ref);
    #endif
			  printf("thr_ratio %f \t granularity_ratio %f\n", thr_ratio, granularity_ratio);

			  if (reset_window(win, thr_ratio, granularity_ratio)) {
			    printf("RESET WINDOW\n");
			    thr_ref = 0.0;
			    granularity_ref = 0.0;
			    clock_timer_start(start_window_reset);
			  }
                        }

		} //end if enabled

	//timer for measurement phase1
	clock_timer_start(w.measurement_phase_timer);
	pthread_mutex_unlock(&stat_mutex);

	} //end if trylock
		

}

#endif
