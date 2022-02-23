#ifndef __WINDOW_H_
#define __WINDOW_H_

#include <events.h>
#include <core.h>
#include <simtypes.h>
#include <stdbool.h>
#include "timer.h"
#include <math.h>
#include "queue.h"


#define INIT_WINDOW_STEP 0.04
#define DECREASE_PERC 0.2
#define INCREASE_PERC 0.5
#define THROUGHPUT_DRIFT 0.05

#define THROUGHPUT_UPPER_BOUND 0.9
#define GRANULARITY_UPPER_BOUND 1.4

#define MEASUREMENT_PHASE_THRESHOLD_MS 500



typedef struct window {

	int enabled; //flag to enable window management (initially zero)
	int direction; //ascent or descent direction for window resizing

	simtime_t size; //window size
	simtime_t step; //step of resizing operations

	double old_throughput; //previous throughput used for the resizing operation

	double perc_increase; //percentage increase for enlarging the window
	bool enlarged; //set to true if the window was enlarged, used for modifying perc_increase

} window;


/* init window parameters*/
static inline void init_window(window *w) {

	w->enabled = 0;
	w->direction = 1;

	w->size = 0.0;

	w->step = nbcalqueue->hashtable->bucket_width * n_prc_tot; //tempo di interarrivo medio (per ora come costante)


	w->old_throughput = 0.0;
	w->perc_increase = 1.0;
	w->enlarged = false;

}



/*The function sets the window size to zero if the reset condition is met
@param: w The window
It returns true if the reset took place, false otherwise
*/
static inline bool reset_window(window *w, double thr_ratio, double avg_granularity_ratio) {


#if VERBOSE == 1
	printf("THROUGHPUT RATIO %f \t GRANULARITY RATIO %f \n", thr_ratio, avg_granularity_ratio);


	printf("thr_ratio < THROUGHPUT_UPPER_BOUND %d\n", thr_ratio < THROUGHPUT_UPPER_BOUND);
	printf("(avg_granularity_ratio < GRANULARITY_UPPER_BOUND) %d\n", (avg_granularity_ratio < GRANULARITY_UPPER_BOUND));
#endif

	//check reset condition
	if ( (thr_ratio < THROUGHPUT_UPPER_BOUND) ||  !(avg_granularity_ratio < GRANULARITY_UPPER_BOUND) )  {
		w->size = 0.0; //reset
		w->step = nbcalqueue->hashtable->bucket_width * n_prc_tot;
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
static inline int window_resizing(window *w, double throughput) {

	int res = -1;



	#if VERBOSE == 1
		printf("NEW THROUGHPUT %f - OLD THROUGHPUT %f \n", throughput, w->old_throughput);
	#endif

	//compute_percentage(&w->perc_increase, w->enlarged);

	#if VERBOSE == 1
		printf("window size before resizing %f\n", w->size);
	#endif

	w->step = nbcalqueue->hashtable->bucket_width * n_prc_tot;


	//first step increment if window size is zero
	if (w->size == 0) {
		w->size = nbcalqueue->hashtable->bucket_width;;
		res = 0;
		w->old_throughput = throughput;
		return res;
	}



	//if throughput has increased check the direction
	if ( (w->old_throughput != 0) && (throughput - w->old_throughput)/w->old_throughput >= THROUGHPUT_DRIFT) { 
		
		if (w->direction > 0) { //enlarge window if previous enlargement caused an increase in throughput
			w->step += w->step*INCREASE_PERC;
			w->size += w->step;
			w->enlarged = true;
		} else { //decrease window to move closer to maximum
			w->step -= w->step*DECREASE_PERC;
			w->size -= w->step;
			if (w->size < 0) w->size = nbcalqueue->hashtable->bucket_width;;
			w->enlarged = false;
		}

	} else if (throughput - w->old_throughput < 0) {
		
		if (w->direction > 0) { //decrease window because previous enlargement caused a decrease in throughput
			w->step -= w->step*DECREASE_PERC;
			w->size -= w->step;
			if (w->size < 0) w->size = nbcalqueue->hashtable->bucket_width;;
			w->enlarged = false;
		} else { //decrease window to move closer to maximum
			w->step -= w->step*DECREASE_PERC;
			w->size -= w->step;
			if (w->size < 0) w->size = nbcalqueue->hashtable->bucket_width;;
			w->enlarged = false;
		}

		w->direction = w->direction * (-1);

	}

	
	w->old_throughput = throughput;


	#if VERBOSE == 1
		printf("window size after resizing %f\n", w->size);
	#endif
	

	return res;

}


#endif