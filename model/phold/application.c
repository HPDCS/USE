#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>

#include <lookahead.h>

#include "application.h"


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	int 	i, j = 123;
	event_content_type new_event;

	lp_state_type *state_ptr = (lp_state_type*)state;


	if(state_ptr != NULL)
		state_ptr->lvt = now;


	switch (event_type) {

		case INIT:
			// Initialize LP's state
			state_ptr = (lp_state_type *)malloc(sizeof(lp_state_type));
                        if(state_ptr == NULL){
                                exit(-1);
                        }

			// Explicitly tell ROOT-Sim this is our LP's state
                        SetState(state_ptr);


			state_ptr->events = 0;

			if(me == 0) {
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
//			for(i = 0; i < 10; i++) {
				timestamp = (simtime_t) (20 * Random());
				if(timestamp < LOOKAHEAD)
					timestamp += LOOKAHEAD;
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
//			}

			break;

		case EXTERNAL_LOOP:
		case LOOP:
			for(i = 0; i < LOOP_COUNT; i++) {
				pow(i, j);
			//	j = i*i;
			}

			state_ptr->events++;

			delta = (simtime_t)(Expent(TAU));
			if(delta < LOOKAHEAD)
				delta += LOOKAHEAD;
			timestamp = now + delta;

			if(event_type == LOOP)
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			if(event_type == LOOP && Random() < 0.2) {
				ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
			}
			break;


		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {

	if(snapshot->events < COMPLETE_EVENTS)
//	if(snapshot->lvt < COMPLETE_TIME)
		return false;
	return true;
}

