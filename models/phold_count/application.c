#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"
#include "hpdcs_utils.h"

//#include <timer.h>

//lp_state_type states[4] __attribute__((aligned (64)));


#define SAME_TS 1

typedef struct model_parameters{
	unsigned int num_events;
}
model_parameters;

struct argp_option model_options[] = {
  {"num-events",        1002, "VALUE", 0, "Number of event per process to end the simulation"               , 0 },
  { 0, 0, 0, 0, 0, 0} 
};

model_parameters args = {
	.num_events = 5000,
};

error_t model_parse_opt(int key, char *arg, struct argp_state *state){
	(void)state;
	switch(key){
		case 1002:
			args.num_events = atoi(arg);
			break;
	}
	return 0;
}

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	unsigned int 	i, j = 123;
	//event_content_type new_event;
	unsigned int loops; 
	lp_state_type *state_ptr = (lp_state_type*)state;
	
	//timer tm_ex;

	if(state_ptr != NULL)
		state_ptr->lvt = now;

	(void)event_content;
	(void)size;
		
	//printf("EVENT INIT: %d\n", INIT);	
	//printf("EVENT TYPE: %d\n", event_type);


	switch (event_type) {

		case INIT:
			state_ptr = malloc(sizeof(lp_state_type));
			if(state_ptr == NULL) {
				printf("malloc failed\n");
				exit(-1);
			}

            if(state_ptr == NULL){
				printf("%d- APP: LP state allocation failed: (%s)\n", me, strerror(errno));
				exit(-1);
            }

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;
			state_ptr->num_events = args.num_events;

			if(me == 0) {
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
			timestamp = 1.0;
		#if SAME_TS == 0
			for(i = 0; i < EVENTS_PER_LP; i++) {
				timestamp = (simtime_t) LOOKAHEAD + (TAU * Random());
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}
		#else
			for(i = 0; i < EVENTS_PER_LP; i++) {
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}
		#endif

			break;

		case EXTERNAL_LOOP:
		case LOOP:
			loops = LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();

			for(i = 0; i < loops ; i++) {
					j = i*i;
			}

			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + delta;

			if(event_type == LOOP && state_ptr->events < state_ptr->num_events){
				state_ptr->events++;
				if(now < state_ptr->lvt){
					printf("\x1b[31m""%d- APP: ERROR: event %f received out of order respect %f; last_loop_count: %u\n""\x1b[0m", me, now, state_ptr->lvt, j);
				}
				state_ptr->lvt = now;
				if(state_ptr->events < state_ptr->num_events){
					ScheduleNewEvent((me+1)%n_prc_tot, now+1.0, LOOP, NULL, 0);
				}
			}

			break;


		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	if(snapshot->events < snapshot->num_events) {
        return false;
    }
    printf(GREEN("[%d] LP to the end execution: LVT:%f NUM_EX:%d\n"), me, snapshot->lvt, snapshot->events);
	return true;
}

