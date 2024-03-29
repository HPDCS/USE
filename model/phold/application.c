#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>
#include <lookahead.h>

#include "application.h"


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	unsigned int 	i, j = 123;
	unsigned long long loops, tmp; 
	lp_state_type *state_ptr = (lp_state_type*)state;
	
	(void) me;
	(void) event_content;
	(void) size;
	

	if(state_ptr != NULL)
		state_ptr->lvt = now;
		
	switch (event_type) {

		case INIT:
            
            /* allocate state for the target LP */
            state_ptr = malloc(sizeof(lp_state_type));
			if(state_ptr == NULL) {
				printf("malloc failed\n");
				exit(-1);
			}

			// Explicitly tell this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;

			if(me == 0) {
				/* this print is ok because init is never rolled back */ 
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %lld, %d total events per LP, lookahead %f\n", LOOP_COUNT_US*CLOCKS_PER_US, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
			for(i = 0; i < EVENTS_PER_LP; i++) {
				timestamp = (simtime_t) LOOKAHEAD + (TAU * Random());
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}

			break;

		case EXTERNAL_LOOP:
		case LOOP:
			tmp = CLOCK_READ();
            loops = tmp;
            while( (tmp-loops) < (LOOP_COUNT_US*CLOCKS_PER_US) )
                tmp = CLOCK_READ();

			state_ptr->events++;

			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + delta;

			if(event_type == LOOP)
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			if(event_type == LOOP )
			{
				for(j=0;j<FAN_OUT;j++){
						delta = LOOKAHEAD + Expent(TAU);
						timestamp = now + delta;
						ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
				}
			}

			if(event_type == LOOP && Random() < 0.2) {
				ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
			}

			break;


		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void) me;
	return snapshot->events >= COMPLETE_EVENTS;
}

