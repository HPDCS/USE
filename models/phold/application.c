#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>
#include <lookahead.h>

#include "application.h"


#define LA (50)


typedef struct model_parameters{
	unsigned int event_us;
	unsigned int fan_out;
	unsigned int num_events;
}
model_parameters;

struct argp_option model_options[] = {
  {"event-us",          1000, "TIME",  0, "Event granularity"               , 0 },
  {"fan-out",           1001, "VALUE", 0, "Event fan out"               , 0 },
  {"num-events",        1002, "VALUE", 0, "Number of event per process to end the simulation"               , 0 },
  { 0, 0, 0, 0, 0, 0} 
};

model_parameters args = {
	.event_us = 5,
	.fan_out  = 1,  
	.num_events = 5000,
};

error_t model_parse_opt(int key, char *arg, struct argp_state *state){
	(void)state;
	switch(key){
		case 1000:
			args.event_us = atoi(arg);
			break;
		case 1001:
			args.fan_out = atoi(arg);
			break;
		case 1002:
			args.num_events = atoi(arg);
			break;
	}
	return 0;
}

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
	      state_ptr = malloc(sizeof(lp_state_type));
				if(state_ptr == NULL) {
					printf("malloc failed\n");
					exit(-1);
				}
				// Explicitly tell ROOT-Sim this is our LP's state
        SetState(state_ptr);
				state_ptr->events = 0;
				state_ptr->event_us = args.event_us;
				state_ptr->fan_out = args.fan_out;
				state_ptr->num_events = args.num_events;
			if(me == 0) {
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %lld, %d total events per LP, lookahead %f\n", state_ptr->event_us*CLOCKS_PER_US, state_ptr->num_events, LOOKAHEAD);
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
                        while( (tmp-loops) < (state_ptr->event_us*CLOCKS_PER_US) ){
                            tmp = CLOCK_READ();
                        }
			if((tmp - loops) < (state_ptr->event_us*CLOCKS_PER_US)) printf("error looping less than required\n");

			state_ptr->events++;

			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + delta;
#if DEBUG == 1
			if(timestamp < now){
				printf("ERROR: new ts %f smaller than old ts %f\n", timestamp, now);	
			}
#endif

			if(event_type == LOOP)
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			if(event_type == LOOP )
			{
				for(j=0;j<state_ptr->fan_out;j++){
						delta = LOOKAHEAD + Expent(TAU);
						timestamp = now + delta;
#if DEBUG == 1
						if(timestamp < now){
							printf("ERROR: new ts %f smaller than old ts %f\n", timestamp, now);	
						}
#endif
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
	if(snapshot->events < snapshot->num_events) {
        return false;
    }
	return true;
}

