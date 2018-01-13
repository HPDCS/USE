#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"
#include "hpdcs_utils.h"

//#include <timer.h>

//lp_state_type states[4] __attribute__((aligned (64)));

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	int 	i, j = 123;
	//event_content_type new_event;
	int err;
	unsigned int loops; 
	lp_state_type *state_ptr = (lp_state_type*)state;
	
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
			//err = posix_memalign((void **)(&state_ptr), 64, 64);
			state_ptr = malloc(sizeof(lp_state_type));
			//if(err < 0) {
			//	printf("memalign failed: (%s)\n", strerror(errno));
			//	exit(-1);
			//}

            if(state_ptr == NULL){
				printf("%d- APP: LP state allocation failed: (%s)\n", me, strerror(errno));
				exit(-1);
            }

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;

			if(me == 0) {
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
			//for(i = 0; i < EVENTS_PER_LP; i++) {
			//	timestamp = (simtime_t) LOOKAHEAD + (TAU * Random());
			//	ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			//}
			ScheduleNewEvent(me, 1.0, LOOP, NULL, 0);
			

			break;

		case EXTERNAL_LOOP:
		case LOOP:
			//timer_start(tm_ex);

			loops = LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();

			for(i = 0; i < loops ; i++) {
					j = i*i;
			}
			//printf("timer: %d\n", timer_value_micro(tm_ex));


			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + delta;

			if(event_type == LOOP && state_ptr->events < COMPLETE_EVENTS){
				state_ptr->events++;
				if(now < state_ptr->lvt){
					printf("\x1b[31m""%d- APP: ERROR: event %f received out of order respect %f\n""\x1b[0m", me, now, state_ptr->lvt);
					state_ptr =NULL;state_ptr->lvt *10;
				}
				//if(state_ptr->lvt == now){
				//	printf("[%d] HO RICEVUTO UN EVENTO CON TS=MY_LVT: event %f INCORRECT STATE %d\n", me, now, state_ptr->events);
				//	state_ptr =NULL;state_ptr->lvt *10;
				//}
				if(state_ptr->events > now){
					printf("%d- APP: ERROR: event %f INCORRECT STATE %d\n""\x1b[0m", me, now, state_ptr->events);
					state_ptr =NULL;state_ptr->lvt *10;
				}
				state_ptr->lvt = now;
				if(state_ptr->events < COMPLETE_EVENTS){
					ScheduleNewEvent((me+1)%n_prc_tot, now+1.0, LOOP, NULL, 0);
				//	ScheduleNewEvent((me+1)%n_prc_tot, timestamp, LOOP, NULL, 0);
				}
				else{
					printf(BLUE("[%d] LP to the end execution: LVT:%f NUM_EX:%d\n"), me, state_ptr->lvt, state_ptr->events);
				}
			
				//for(j=0;j<FAN_OUT;j++){
				//		delta = LOOKAHEAD + Expent(TAU);
				//		timestamp = now + delta;
				//		ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
				//}
			}

			//if(event_type == LOOP && Random() < 0.2) {
			//	ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
			//}

			break;


		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	
	//printf("TOTALE: %u\n", snapshot->events);//da_cancellare

	if(snapshot->events < COMPLETE_EVENTS) {
//	if(snapshot->lvt < COMPLETE_TIME) {
        return false;
    }
	return true;
}

