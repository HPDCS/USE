#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"

//#include <timer.h>

//lp_state_type states[4] __attribute__((aligned (64)));

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	volatile unsigned int i, j = 123;
	//event_content_type new_event;
	unsigned int loops;
	event_content_type *payload = NULL;
	lp_state_type *state_ptr = (lp_state_type*)state;
	
	(void) me;
	(void) event_content;
	(void) size;
	
	//timer tm_ex;

	if(state_ptr != NULL)
		state_ptr->lvt = now;
	//printf("EVENT INIT: %d\n", INIT);	
	//printf("EVENT TYPE: %d\n", event_type);


	switch (event_type) {

		case INIT:
			// Initialize LP's state
			//state_ptr = (lp_state_type *)malloc(sizeof(lp_state_type));

			//if(states[me] == NULL)
			//	state_ptr = states[me];

			//Allocate a pointer of 64 bytes aligned to 64 bytes (cache line size)
		//	err = posix_memalign((void **)(&state_ptr), 64, 64);
		//	if(err < 0) {
		//		printf("memalign failed: (%s)\n", strerror(errno));
		//		exit(-1);
		//	}
		//
        //    if(state_ptr == NULL){
		//		printf("LP state allocation failed: (%s)\n", strerror(errno));
		//		exit(-1);
        //    }
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
			
			for(i = 0; i < EVENTS_PER_LP; i++) {
				timestamp = (simtime_t) LOOKAHEAD + (TAU * Random());
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}

			break;

		case EXTERNAL_LOOP:
		case LOOP:
			//timer_start(tm_ex);

			if (event_content != NULL)
			{
				loops = event_content->grain * LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();
				// free(event_content);
			}
			else
				loops = LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();

			for(i = 0; i < loops ; i++) {
					j = i*i;
			}
			//printf("timer: %d\n", timer_value_micro(tm_ex));

			state_ptr->events++;

			// delta = LOOKAHEAD + Expent(TAU);
			// timestamp = now + delta;
#if DEBUG == 1
			if(timestamp < now){
				printf("ERROR: new ts %f smaller than old ts %f\n", timestamp, now);	
			}
#endif
			// if(event_type == LOOP)
			// 	ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			if(event_type == LOOP )
			{
				for(j=0;j<FAN_OUT;j++){
						delta = LOOKAHEAD + Expent(TAU);
						timestamp = now + delta;
#if DEBUG == 1
						if(timestamp < now){
							printf("ERROR: new ts %f smaller than old ts %f\n", timestamp, now);	
						}

						payload = (event_content_type *) malloc(sizeof(event_content_type));
						payload->grain = (unsigned int) delta;
#endif
						ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, (void*)payload, sizeof(event_content_type));
				}
			}

			// if(event_type == LOOP && Random() < 0.2) {
			// 	ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
			// }

			break;


		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void) me;
	//printf("TOTALE: %u\n", snapshot->events);//da_cancellare

	if(snapshot->events < COMPLETE_EVENTS) {
//	if(snapshot->lvt < COMPLETE_TIME) {
        return false;
    }
	return true;
}

