#if ENFORCE_LOCALITY == 1
#include <timer.h>
#include <core.h>
#include <pthread.h>
#include <stdbool.h>
#include <statistics.h>
#include <window.h>

window w;
simtime_t *window_size;
int *enabled;

double old_thr, thr_ref;
simtime_t prev_granularity, prev_granularity_ref, granularity_ref;
unsigned int prev_comm_evts, prev_comm_evts_ref, prev_first_time_exec_evts, prev_comm_evts_window, prev_first_time_exec_evts_ref;

__thread clock_timer start_window_reset;
__thread clock_timer start_metrics_interval;
__thread stat64_t time_interval_for_metrics_comput;
__thread stat64_t time_interval_for_window_reset;

pthread_mutex_t stat_mutex, window_check_mtx;


void init_metrics_for_window() {

	init_window(&w);
	window_size = &w.size;
	enabled = &w.enabled;
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


double compute_throughput_for_committed_events(unsigned int *prev_comm_evts, clock_timer timer ,simtime_t time_interval) {

	unsigned int i;
	unsigned int comm_evts = 0;

	for (i = 0; i < n_cores; i++) {
		comm_evts += thread_stats[i].events_committed;
	}
	comm_evts -= *prev_comm_evts;
	*prev_comm_evts = comm_evts;

	time_interval = clock_timer_value(timer);

	return comm_evts/time_interval;

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


void aggregate_metrics_for_window_management(window *w) {

	double current_thr = 0.0, thr_window = 0.0, thr_ratio = 0.0;
	simtime_t granularity = 0.0, granularity_ratio = 0.0;

	if (pthread_mutex_trylock(&stat_mutex) == 0) { //just one thread executes the underlying block

		current_thr = compute_throughput_for_committed_events(&prev_comm_evts ,start_metrics_interval, time_interval_for_metrics_comput);

		//printf("current_thr %f\n", current_thr);
		
		//check if window must be enabled
		//by comparing current thr with the previous one
		if ( !(*enabled) && (old_thr != 0.0) && ( ( current_thr/old_thr <= 1.0) && ( current_thr/old_thr >= 0.9 ) ) ) {
			*enabled = 1;
		} 
		old_thr = current_thr;
		
		granularity = compute_average_granularity(&prev_first_time_exec_evts, &prev_granularity);
		//printf("granularity %f\n", granularity);

		//window is enabled -- workload can be considered stable
		if (*enabled) {
			
			thr_window = compute_throughput_for_committed_events(&prev_comm_evts_window, start_metrics_interval, time_interval_for_metrics_comput);

			//printf("thr_window %f old_thr %f\n", thr_window, w->old_throughput);

			window_resizing(w, thr_window); //do resizing operations -- window might be enlarged, decreased or stay the same

			//printf("window_resizing %f\n", *window_size);
			
			thr_ref = compute_throughput_for_committed_events(&prev_comm_evts_ref, start_window_reset, time_interval_for_window_reset);
			granularity_ref = compute_average_granularity(&prev_first_time_exec_evts_ref, &prev_granularity_ref);

			//printf("thr_ref %f granularity_ref %f\n", thr_ref, granularity_ref);

			thr_ratio = thr_window/thr_ref;
			granularity_ratio = granularity/granularity_ref;

			//printf("thr_ratio %f \t granularity_ratio %f\n", thr_ratio, granularity_ratio);

			if (reset_window(w, thr_ratio, granularity_ratio)) { 
				thr_ref = 0.0;
				granularity_ref = 0.0;
				clock_timer_start(start_window_reset);
			}
		}

	pthread_mutex_unlock(&stat_mutex);
	}
		

}

#endif