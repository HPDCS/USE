#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_

#include <events.h>
#include <simtypes.h>
#include <stdbool.h>

/*
Algorithm: Hill Climbing 
Evaluate the initial state. 
Loop until a solution is found or there are no new operators left to be applied: 
   - Select and apply a new operator 
   - Evaluate the new state: 
      goal -→ quit 
      better than current state -→ new current state 

*/


typedef struct window {

	simtime_t size;
	simtime_t last_reset_time;
	double event_rate_ref;
	double granularity_ref; //granularity must be computed after execution of each event
	double last_comm_event_rate;
	double last_granularity;

} window;



bool window_resizing(window *w, double comm_event_rate, double avg_granularity, simtime_t step) {

	double er = event_rate_ref/comm_event_rate;
	double granularity = avg_granularity/granularity_ref;

	if (comm_event_rate > w->last_comm_event_rate) {

		//check reset condition
		if ((er < 0.9) || (((granularity > 0.6) && (granularity < 1.4)))) {
			w->size = 0; //reset
			//update w->last_reset_time 
		} else {
			//enlarge window
			w->size += step;
			w->last_comm_event_rate = comm_event_rate;
			w->last_granularity = avg_granularity;
			return 1;
		}
	}

	return 0;

}

#endif