#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"

//#include <timer.h>

//lp_state_type states[4] __attribute__((aligned (64)));

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

    unsigned int hotspots;
    double p_hotspot;
    unsigned int phase;

	simtime_t timestamp, delta;
	unsigned int i, j = 123;
	unsigned int loops; 
	lp_state_type *state_ptr = (lp_state_type*)state;
	
	//timer tm_ex;
	(void) event_content;
	(void) size;
	(void) me;
/*
	if(state_ptr != NULL) {
        state_ptr->lvt = now;
        if(me == 0)printf("[%u] count = %u\n", me,  state_ptr->events);
    } else
        if(me == 0)printf("[%u] Initializing\n ", me);
*/
	//printf("EVENT INIT: %d\n", INIT);	
	//printf("EVENT TYPE: %d\n", event_type);


	switch (event_type) {

		case INIT:
			// Initialize LP's state
			//state_ptr = (lp_state_type *)malloc(sizeof(lp_state_type));

			//if(states[me] == NULL)
			//	state_ptr = states[me];

			//Allocate a pointer of 64 bytes aligned to 64 bytes (cache line size)
			state_ptr = malloc(sizeof(lp_state_type));
			
            if(state_ptr == NULL){
				printf("LP state allocation failed: (%s)\n", strerror(errno));
				exit(-1);
            }

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;

			if(me == 0) {
				printf("Running a mutable loop-based hotspotted PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
			for(i = 0; i < EVENTS_PER_LP; i++) {
				timestamp = (simtime_t) LOOKAHEAD + (TAU * Random());
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}

			break;

        case LOOP:
		case EXTERNAL_LOOP:
			//timer_start(tm_ex);

			loops = LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();

			for(i = 0; i < loops ; i++) {
					j = i*i;
			}
			//printf("timer: %d\n", timer_value_micro(tm_ex));

			state_ptr->events++;

			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + delta;
#if DEBUG == 1
			if(timestamp < now){
				printf("ERROR: new ts %f smaller than old ts %f\n", timestamp, now);	
			}
#endif

			if(event_type == LOOP){
				//rimando un primo evento a me stesso
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

                phase = (unsigned int)(state_ptr->events / ((int)((double)COMPLETE_EVENTS/(double)NUM_HOTSPOT_PHASES))) % 2;
                p_hotspot = (double)(phase == 0 ? P_HOTSPOT_PHASE1 : P_HOTSPOT_PHASE2);
                hotspots = (phase == 0 ? HOTSPOTS_PHASE1 : HOTSPOTS_PHASE2);

				//genero eventi di fan-outde
				for(j=0;j<FAN_OUT;j++){
					delta = LOOKAHEAD + Expent(TAU);
					timestamp = now + delta;
					if((hotspots > 0 && Random() < p_hotspot) || hotspots >= n_prc_tot){
						ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH)%hotspots, timestamp, EXTERNAL_LOOP, NULL, 0); //TODO: hotspot+1?
					}
					else{
						ScheduleNewEvent(hotspots+FindReceiver(TOPOLOGY_MESH)%(n_prc_tot-hotspots), timestamp, EXTERNAL_LOOP, NULL, 0);
					}
				}
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
	
	(void) me;
	
	//printf("TOTALE: %u\n", snapshot->events);//da_cancellare

	if(snapshot->events < COMPLETE_EVENTS) {
//	if(snapshot->lvt < COMPLETE_TIME) {
        return false;
    }
	return true;
}

