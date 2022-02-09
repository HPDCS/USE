#ifndef __WINDOW_H_
#define __WINDOW_H_

#include <events.h>
#include <core.h>
#include <simtypes.h>
#include <stdbool.h>
#include "timer.h"
#include <math.h>


#define INIT_WINDOW_STEP 0.04
#define DECREASE_PERC 0.05
#define THROUGHPUT_DRIFT 0.05

#define THROUGHPUT_UPPER_BOUND 0.9
#define GRANULARITY_LOWER_BOUND 0.6
#define GRANULARITY_UPPER_BOUND 1.4


typedef struct window {

	simtime_t size; //window size
	simtime_t time_interval; //observation interval
	simtime_t step; //step of resizing operations

	double throughput; //throughput per-thread
	double throughput_ref; //reference throughput
	double thr_ratio; //throughput ratio
	double granularity_ratio; //granularity_ratio of events
	double old_throughput; //previous throughput used for the resizing operation

	double perc_increase; //percentage increase for enlarging the window
	bool enlarged; //set to true if the window was enlarged, used for modifying perc_increase

} window;


/* init window parameters*/
static inline void init_window(window *w) {

	w->size = 0.0;

	w->step = INIT_WINDOW_STEP; //tempo di interarrivo medio (per ora come costante)
	w->time_interval = 0.0;

	w->granularity_ratio = 0.0;
	w->throughput = 0.0;
	w->throughput_ref = 0.0;
	w->thr_ratio = 0.0;

	w->old_throughput = 0;
	w->perc_increase = 1.0;
	w->enlarged = false;

}


/* The function computes the granularity ratio 
@param: w The window
@param: sum_granularity The sum of granularities of events
@param: granularity_ref The granularity of events computed from last window reset
*/
static inline void compute_granularity_ratio(window *w, double sum_granularity, double granularity_ref) {

	w->granularity_ratio = sum_granularity / granularity_ref; 

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


/* The function computes the reference throughput for committed events 
@param: w The window
@param: comm_evts_ref The committed events counted from last window reset
@param: time_interval The time observation period since the last window reset
*/
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
	double granularity_ratio = w->granularity_ratio;

#if DEBUG == 1
	printf("THROUGHPUT RATIO %f \t GRANULARITY RATIO %f \n", throughput_ratio, granularity_ratio);
#endif

	//check reset condition
	if ((throughput_ratio < THROUGHPUT_UPPER_BOUND) || (((granularity_ratio > GRANULARITY_LOWER_BOUND) && (granularity_ratio < GRANULARITY_UPPER_BOUND)))) {
		w->size = 0.0; //reset
		w->step = INIT_WINDOW_STEP;
		return true;
	}

	return false;
}


/*The function sets the new percentage for increasing the window
@param: perc_increase The current percentage
@param: enlarged Boolean value to signal a previous enlargement of the window
*/
static inline void compute_percentage(double *perc_increase, bool enlarged) {

	*perc_increase = (enlarged) ? *perc_increase/2.0 : 1.0;

}


/* The function adapts the window size based on the committed events
@param: w The window
@param: comm_evts The committed events at the current observation time
It returns 0 if the window was updated for the first time after a reset, 
			  1 if the window was enlarged
			  2 if the window was decreased
			 -1 if the window was not updated
*/
static inline int window_resizing(window *w) {

	double increase, decrease;
	int res = -1;

	#if VERBOSE == 1
		printf("NEW THROUGHPUT %f - OLD THROUGHPUT %f \n", w->throughput, w->old_throughput);
	#endif

	compute_percentage(&w->perc_increase, w->enlarged);

	//first step increment if window size is zero
	if (w->size == 0) {
		w->size += w->step;
		res = 0;
		return res;
	}
	
	#if VERBOSE == 1
		printf("window size before resizing %f\n", w->size);
	#endif

	//se ho tanti aumenti di meno del 5% e quindi non incremento mai la finestra?

	//enlarge window if throughput increases
	if ((w->throughput - w->old_throughput)/w->old_throughput >= THROUGHPUT_DRIFT) { 
		increase = w->step + w->step*w->perc_increase;
		w->size += increase;
		w->enlarged = true;
		res = 1;
	} else if (abs((w->throughput - w->old_throughput)/w->old_throughput) >= THROUGHPUT_DRIFT) {
		decrease = w->step - w->step*DECREASE_PERC;
		w->size -= decrease;
		if (w->size < 0) w->size = INIT_WINDOW_STEP;
		w->enlarged = false;
		res = 2;
	}

	w->old_throughput = w->throughput;


	#if VERBOSE == 1
		printf("window size after resizing %f\n", w->size);
	#endif
	

	return res;

}


#endif