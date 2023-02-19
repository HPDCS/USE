//#if ENFORCE_LOCALITY == 1
#include <timer.h>
#include <core.h>
#include <pthread.h>
#include <stdbool.h>
#include <statistics.h>
#include <window.h>
#include <clock_constant.h>
#include <math.h>

window w;


#define NUM_THROUGHPUT_SAMPLES 10

double th_samples[NUM_THROUGHPUT_SAMPLES];
double stability_samples[NUM_THROUGHPUT_SAMPLES];
unsigned long long stability_counter = 0;
unsigned long long th_counter = 0;
unsigned long long start_simul_time = 0;

static double old2_thr, old_thr, thr_ref;
static simtime_t prev_granularity, prev_granularity_ref, granularity_ref;
static unsigned int prev_comm_evts, prev_comm_evts_ref, prev_first_time_exec_evts, prev_comm_evts_window, prev_first_time_exec_evts_ref;


static pthread_mutex_t stat_mutex, window_check_mtx;


static __thread clock_timer start_window_reset;
static __thread stat64_t time_interval_for_measurement_phase;
static __thread double elapsed_time;
extern unsigned int rounds_before_unbalance_check; /// number of window management rounds before checking numa unbalance


__thread simtime_t MAX_LOCAL_DISTANCE_FROM_GVT = START_WINDOW;




int get_mem_usage() {
    FILE *statFP;
    int oldNumbers[7];
//    int newNumbers[7];
//    int diffNumbers[7];
    char cpu[10];  // Not used

    statFP = fopen("/proc/self/statm", "r");

    fscanf(statFP,
            "%s %d %d %d %d %d %d %d",
            cpu,
            &oldNumbers[0], 
            &oldNumbers[1],
            &oldNumbers[2],
            &oldNumbers[3],
            &oldNumbers[4],
            &oldNumbers[5],
            &oldNumbers[6]);

    fclose(statFP);

    return oldNumbers[1];
}













void init_metrics_for_window() {

	init_window(&w);
	pthread_mutex_init(&stat_mutex, NULL);
	pthread_mutex_init(&window_check_mtx, NULL);
	clock_timer_value(start_simul_time);
	start_simul_time = CLOCK_READ();
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
  printf("thr_ratio %f \t granularity_ratio %f th %f thref %f gr %f ts %llu\n", 0.0, 0.0, 0.0, 0.0, 0.0, (start_simul_time)/CLOCKS_PER_US/1000);
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
	//comm_evts=0;
        //for(i=0;i<n_prc_tot;i++)
	{
        //    comm_evts += lp_stats[i].events_total - lp_stats[i].events_silent;
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
	int skip = 0;
	current_thr = compute_throughput_for_committed_events(&prev_comm_evts ,w.measurement_phase_timer);
	if(current_thr == 0.0) return;
	if(isnan(current_thr)) return;
	if(isinf(current_thr)) return;

#if VERBOSE == 1
	printf("current_thr/old_thr %f\n", current_thr/old_thr);
#endif

	//check if window must be enabled
	//by comparing current thr with the previous one
	stability_samples[stability_counter % NUM_THROUGHPUT_SAMPLES] = current_thr;
	stability_counter++;

	if(stability_counter < NUM_THROUGHPUT_SAMPLES) return;
	

	double avg = 0.0;
	double std = 0.0;
	for(int i=0;i<NUM_THROUGHPUT_SAMPLES;i++) avg += stability_samples[i];
	avg/=NUM_THROUGHPUT_SAMPLES;
	for(int i=0;i<NUM_THROUGHPUT_SAMPLES;i++) std += (avg-stability_samples[i])*(avg-stability_samples[i]);
	std/=NUM_THROUGHPUT_SAMPLES-1;
	std = sqrt(std);
	skip = skip || ( current_thr < (avg-std*3) );
	skip = skip || ( current_thr > (avg+std*3) );
	if(skip) return;

	if(old2_thr != 0.0){
		diff1 = old2_thr/current_thr;
		diff2 = old_thr/current_thr;
		if(diff1 < (1.0+THROUGHPUT_DRIFT)  && diff2 < (1.0+THROUGHPUT_DRIFT) && diff1 > (1.0-THROUGHPUT_DRIFT) && diff2 > (1.0-THROUGHPUT_DRIFT))  w.enabled = 1;
	}
	printf("stability check: %2.f%% %2.f%%\n", 100*diff1, 100*diff2);
	
    if(!w.enabled && current_thr > 0.0)         
			  printf("thr_ratio %f \t granularity_ratio %f th %f thref %f gr %f ts %llu\n", 0.0, 0.0, current_thr, 0.0, 0.0, (CLOCK_READ()-start_simul_time)/CLOCKS_PER_US/1000);
	old2_thr = old_thr;
	old_thr = current_thr;

}



void aggregate_metrics_for_window_management(window *win) {
// #if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 0
//   return;
// #endif
	double thr_window = 0.0, thr_ratio = 0.0;
	simtime_t granularity = 0.0, granularity_ratio = 0.0;
	time_interval_for_measurement_phase = clock_timer_value(w.measurement_phase_timer);
	elapsed_time = time_interval_for_measurement_phase / CLOCKS_PER_US;
	if (elapsed_time/1000.0 < MEASUREMENT_PHASE_THRESHOLD_MS) return;

	if (pthread_mutex_trylock(&stat_mutex) == 0) { //just one thread executes the underlying block

		++rounds_before_unbalance_check;
		if (!w.enabled) enable_window(&old_thr);
		
		//window is enabled -- workload can be considered stable
		if (w.enabled) {
			
			thr_window = compute_throughput_for_committed_events(&prev_comm_evts_window, w.measurement_phase_timer);
			granularity = compute_average_granularity(&prev_first_time_exec_evts, &prev_granularity);
                    #if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 1
			//fprintf(stderr, "SIZE BEFORE RESIZING %f\n", *window_size);
			window_resizing(win, thr_window); //do resizing operations -- window might be enlarged, decreased or stay the same
                    #else
                        if(w.phase == 0) w.phase = 1;
                    #endif  
                        
	#if VERBOSE == 1
			printf("thr_window %f granularity %f\n", thr_window, granularity);
			printf("window_resizing %f\n", w.size);
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
			  printf("thr_ratio %f \t granularity_ratio %f th %f thref %f gr %f ts %llu\n", thr_ratio, granularity_ratio, thr_window, thr_ref, granularity, (CLOCK_READ()-start_simul_time)/CLOCKS_PER_US/1000);
                        #if ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF == 1 // use moving average only when dynamic sizing is enabled"
                          //if(thr_window == thr_window) //check for NaN float // TODO investigate this
                          //  thr_ref = 0.6*thr_ref + 0.4*thr_window;
                          // thr_ref = 1.0*thr_ref + 0.0*thr_window;
			  //else
			   //thr_ref = thr_ref;
                        #endif
			  if(thr_window == 0.0) goto end;
			  if(isnan(thr_window)) goto end;
			  if(isinf(thr_window)) goto end;
			  int skip = 0;
                           double avg = 0.0;
		  	if(th_counter >= NUM_THROUGHPUT_SAMPLES){
		  		//double avg = 0.0;
		  		double std = 0.0;
		  		for(int i=0;i<NUM_THROUGHPUT_SAMPLES;i++) avg += th_samples[i];
		  		avg/=NUM_THROUGHPUT_SAMPLES;
		  		for(int i=0;i<NUM_THROUGHPUT_SAMPLES;i++) std += (avg-th_samples[i])*(avg-th_samples[i]);
		  		std/=NUM_THROUGHPUT_SAMPLES-1;
		  	  std = sqrt(std);
		  	  skip = skip || ( thr_window < (avg-std*3) );
		  	  skip = skip || ( thr_window > (avg+std*3) );
    #if VERBOSE == 1
		  	  printf("Checking sample AVG:%f STD:%f TH:%f SKIP:%d\n", avg, std, thr_window, skip);
    #endif
		  	}
			  	
		  	th_samples[th_counter%NUM_THROUGHPUT_SAMPLES] = thr_window;
				th_counter++;
			  	
		  	if (!skip && ENABLE_DYNAMIC_SIZING_FOR_LOC_ENF && reset_window(win, thr_ratio, granularity_ratio)) {
			    printf("RESET WINDOW\n");
			    thr_ref = 0.0;
			    granularity_ref = 0.0;
			    rounds_before_unbalance_check = 0;
			    clock_timer_start(start_window_reset);
			  }
			if(!skip && avg > (thr_ref*1.05) && avg < (thr_ref*2.20)) thr_ref = avg;	
      }

		} //end if enabled
end:
	//timer for measurement phase1
	clock_timer_start(w.measurement_phase_timer);
	pthread_mutex_unlock(&stat_mutex);

	} //end if trylock
		

}

//#endif
