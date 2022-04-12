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
#define THROUGHPUT_DRIFT 0.025

<<<<<<< Updated upstream
#define THROUGHPUT_UPPER_BOUND 0.9
=======
#define THROUGHPUT_UPPER_BOUND 0.95
>>>>>>> Stashed changes
#define GRANULARITY_UPPER_BOUND 1.4

#define MEASUREMENT_PHASE_THRESHOLD_MS 500



typedef struct window {

	int enabled; //flag to enable window management (initially zero)
	simtime_t direction; //ascent or descent direction for window resizing

	simtime_t size; //window size
	simtime_t step; //step of resizing operations

	double old_throughput; //previous throughput used for the resizing operation

	double perc_increase; //percentage increase for enlarging the window
	bool enlarged; //set to true if the window was enlarged, used for modifying perc_increase
	clock_timer measurement_phase_timer;
	int phase;

} window;


/* init window parameters*/
static inline void init_window(window *w) {

	w->enabled = 0;
	w->direction = 1.0;
	w->size = 0.0;
	w->step = 0.0; //nbcalqueue->hashtable->bucket_width * n_prc_tot; //tempo di interarrivo medio (per ora come costante)
	w->old_throughput = 0.0;
	w->perc_increase = 1.0;
	w->enlarged = false;
	clock_timer_start(w->measurement_phase_timer);

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
		w->size  = 0.0; //reset
		w->step  = nbcalqueue->hashtable->bucket_width * n_prc_tot/2;
                w->phase = 0;
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
	double old_throughput = w->old_throughput;
	double th_ratio = throughput/old_throughput;
	simtime_t old_wsize = w->size;
	if(throughput==0.0) return res;
        if(w->enabled && w->phase >= 1) return res;
	#if VERBOSE == 1
		printf("NEW THROUGHPUT %f - OLD THROUGHPUT %f \n", throughput, w->old_throughput);
	#endif

	//compute_percentage(&w->perc_increase, w->enlarged);

	#if VERBOSE == 1
		printf("window size before resizing %f\n", w->size);
	#endif

	w->step = nbcalqueue->hashtable->bucket_width * n_prc_tot/2;


	//first step increment if window size is zero
	if (w->size == 0) {
	  //w->size = nbcalqueue->hashtable->bucket_width;;
	  w->size = w->step;
	  res = 0;
	  w->direction = 1.0;
	  w->old_throughput = throughput;
	  w->phase = 0;
	  printf("starting windows size search %f %f phase:%d\n", 0.0, w->size, w->phase);
	  return res;
	}

	if(w->old_throughput != 0){

          if( (fabs(th_ratio-1)) < THROUGHPUT_DRIFT) {return res;}
<<<<<<< Updated upstream
	  else if (throughput < old_throughput){
=======
	  else 
	  if (throughput < old_throughput){
>>>>>>> Stashed changes
            printf("changing direction %f %f\n", throughput, old_throughput);
            w->direction = w->direction * (-1);
            w->phase=1;
          }
	  w->size	+= w->step*w->direction;
        }
	if(w->size < 0) w->size = 0;
	w->old_throughput = throughput;

      #if VERBOSE == 1
        printf("window size after resizing %f %f %f\n", w->size, throughput, old_throughput);
      #endif
	printf("cur %f th_drift %.2f%% %.2f%% new %f %d\n", old_wsize , (th_ratio-1)*100.0, (fabs(th_ratio-1))*100.0, w->size, w->phase);

	return res;

}


#endif
