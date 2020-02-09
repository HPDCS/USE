#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp;
	unsigned volatile int i,j = USELESS_INIT_NUMBER;
	unsigned volatile int loops;

	lp_state_type *state_ptr = (lp_state_type*)state;
	(void) me;
	(void) event_content;
	(void) size;

	if(state_ptr != NULL)
		state_ptr->lvt = now;

	switch (event_type) {

		case INIT:
			//TODO check all parameters
            state_ptr = malloc(sizeof(lp_state_type));
			if(state_ptr == NULL) {
				printf("malloc failed\n");
				exit(-1);
			}

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;

			if(me == 0) {
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			//TODO print all parameters

			timestamp =  (simtime_t)(me + n_prc_tot) ;// ts normal_evt==i+NUM_LP,ts is integer
			ScheduleNewEvent(me, timestamp, NORMAL_EVT, NULL, 0);

			break;

		case NORMAL_EVT:

			//TODO debug ts is integer
			
			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * Random();
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			state_ptr->events++;//num NORMAL_EVT executed

			ScheduleNewEvent(me, now+n_prc_tot, NORMAL_EVT, NULL, 0);//it generates my next evt
			
			for(i = 0; i < n_prc_tot; i++){
				if(i!=(unsigned int)me && THRESOLD_PROB_NORMAL >= Random() ){
					timestamp=(simtime_t)(now+SHIFT);
					ScheduleNewEvent(i, timestamp, ABNORMAL_EVT, NULL, 0);
				}
			}

			break;

		case ABNORMAL_EVT:

			//TODO debug ts-SHIFT is integer

			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * Random();
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			break;

		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void) me;

	if(snapshot->events < COMPLETE_EVENTS) {
        return false;
    }
	return true;
}

