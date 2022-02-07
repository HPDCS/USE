#ifndef __WINDOW_H_
#define __WINDOW_H_

#include <events.h>
#include <core.h>
#include <simtypes.h>
#include <stdbool.h>
#include "timer.h"


typedef struct window {

	simtime_t size;
	simtime_t time_interval;
	simtime_t step;
	double throughput;
	double throughput_ref;
	double thr_ratio;
	double granularity; //granularity must be computed after execution of each event
	double last_comm_rate;

} window;


/* init window parameters*/
static inline void init_window(window *w) {

	w->size = 0.0;

	w->step = 0.1;
	w->time_interval = 0.0;

	w->granularity = 0.0;
	w->throughput = 0.0;
	w->throughput_ref = 0.0;
	w->thr_ratio = 0.0;

	w->last_comm_rate = 0;

}


/* The function computes the granularity ratio 
@param: w The window
@param: sum_granularity The sum of granularities of events
@param: granularity_ref The granularity of events computed from last window reset
*/
static inline void compute_granularity(window *w, double sum_granularity, double granularity_ref) {

	w->granularity = sum_granularity / granularity_ref; 

} 


/* The function computes the throughput for committed events 
@param: w The window
@param: time_interval The time observation period
@param: comm_evts The committed events at the current observation time
*/
static inline void compute_throughput(window *w, simtime_t time_interval, unsigned int comm_evts) {

	w->throughput = comm_evts/time_interval;

	#if VERBOSE == 1
		printf("THROUGHPUT %f \n", w->throughput);
	#endif

}


static inline void compute_throughput_ref(window *w, unsigned int comm_evts_ref, simtime_t time_interval) {

	w->throughput_ref = comm_evts_ref/time_interval;
	#if VERBOSE == 1
		printf("THROUGHPUT REF %f\n", w->throughput_ref);
	#endif
}



/*The function sets the window size to zero if the reset condition is met
@param: w The window
It returns true if the reset took place, false otherwise
*/
static inline bool reset_window(window *w) {

	double throughput_ratio = w->throughput / w->throughput_ref;
	double granularity = w->granularity;

#if DEBUG == 1
	printf("THROUGHPUT RATIO %f \n", throughput_ratio);
#endif

	//check reset condition
	if ((throughput_ratio < 0.9) || (((granularity > 0.6) && (granularity < 1.4)))) {
		w->size = 0.0; //reset
		w->step = 0.1;
		return true; 
	}

	return false;
}



/* The function adapts the window size based on the committed events
@param: w The window
@param: comm_evts The committed events at the current observation time
It returns 0 if the window was updated for the first time after a reset, 
			  1 if the window was updated with a 50% increase
			 -1 if the window was not updated
*/
static inline int window_resizing(window *w) {

	double increase;
	int res = -1;

	#if VERBOSE == 1
		printf("COMM EVTS %f - LAST COMM EVTS %f \n", w->throughput, w->last_comm_rate);
	#endif

	//first step increment if window size is zero
	if (w->size == 0) {
		w->size += w->step;
		res = 0;
		return res;
	}
	

	//enlarge window after modifying the step of increase
	if (w->throughput > w->last_comm_rate) {
		increase = w->step*0.5;
		w->step += increase;
		w->size += w->step;
		w->last_comm_rate = w->throughput;
		res = 1;
	}

	#if VERBOSE == 1
		printf("window size after increment %f\n", w->size);
	#endif
	

	return res;

}


#endif