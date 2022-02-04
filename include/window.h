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
	double granularity; //granularity must be computed after execution of each event
	unsigned int last_comm_evts;

} window;


/* init window parameters*/
static inline void init_window(window *w) {

	w->size = 0.0;

	w->step = 0.1;
	w->time_interval = 0.0;

	w->granularity = 0.0;
	w->throughput = 0.0;

	w->last_comm_evts = 0;

}




/* The function computes window management metrics for resizing purposes
@param: w The window
@param: sum_granularity The granularity at the current observation time
@param: granularity_ref The granularity computed from the last window reset
@param: time_interval The time observation period
@param: comm_evts The committed events at the current observation time
@param: comm_evts_ref The committed events computed from the last window reset
*/
static inline void compute_metrics(window *w, double sum_granularity, double granularity_ref, simtime_t time_interval, unsigned int comm_evts, unsigned int comm_evts_ref) {

	granularity_ref += sum_granularity;
	comm_evts_ref += comm_evts;

	w->granularity = sum_granularity / granularity_ref;
	w->throughput = comm_evts_ref/time_interval;
	
	/*#if DEBUG == 1
		printf("THROUGHPUT %f - GRANULARITY %f \n", w->throughput, w->granularity);
	#endif*/

}



/*The function sets the window size to zero if the reset condition is met
@param: w The window
It returns true if the reset took place, false otherwise
*/
static inline bool reset_window(window *w) {

	double throughput = w->throughput;
	double granularity = w->granularity;


	//check reset condition
	if ((throughput < 0.9) || (((granularity > 0.6) && (granularity < 1.4)))) {
		w->size = 0.0; //reset
		w->step = 0.1;
		return true; 
	}

	return false;
}

/* The function adapts the window size based on the committed events
@param: w The window
@param: comm_evts The committed events at the current observation time
It returns the current size of the window
*/
static inline simtime_t window_resizing(window *w, unsigned int comm_evts) {

	double increase;

	#if VERBOSE == 1
		printf("COMM EVTS %d - LAST COMM EVTS %u \n", comm_evts, w->last_comm_evts);
	#endif

	//first step increment if window size is zero
	if (w->size == 0) w->size += w->step;
	
	increase = w->step*0.5;
	w->step += increase;

	//enlarge window after modifying the step of increase
	if (comm_evts > w->last_comm_evts) w->size += w->step;

	#if VERBOSE == 1
	printf("window size after increment %f\n", w->size);
	#endif
	
	w->last_comm_evts = comm_evts;

	//check if a reset must take place
	if (reset_window(w)) return w->size;

	return w->size;

}


#endif