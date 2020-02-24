#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	unsigned int i, j = 123;
	unsigned int receiver;
	unsigned long long int loops;
	lp_state_type *state_ptr = (lp_state_type*)state;
	
	(void) me;
	(void) event_content;
	(void) size;
	
	if(state_ptr != NULL)
		state_ptr->lvt = now;


	switch (event_type) {

		case INIT:

            state_ptr = malloc(sizeof(lp_state_type));
			if (state_ptr == NULL)
			{
				printf("malloc failed\n");
				exit(-1);
			}

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			state_ptr->events = 0;
			state_ptr->token = (me == 0) ? 1 : 0;

			if (me == 0)
			{
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}

			timestamp = LOOKAHEAD + Expent(TAU);
			ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			break;

		case PASS_TOKEN_LOOP:

			state_ptr->token -= 1;

			loops = TOKEN_LOOP_COUNT;
			for(i = 0; i < loops ; i++)
				j = j + i;

			state_ptr->events++;

			timestamp = now + 0.000001;
			
			for(j=0; j<FAN_OUT; j++)
			{
				do { receiver = FindReceiver(TOPOLOGY_MESH); } while (receiver == me);
				ScheduleNewEvent(receiver, timestamp, EXTERNAL_LOOP, NULL, 0);
			}

			timestamp = now + 0.000002;

			do { receiver = FindReceiver(TOPOLOGY_MESH); } while (receiver == me);
			ScheduleNewEvent(receiver, timestamp, RECEIVE_TOKEN_LOOP, NULL, 0);

			break;

		case RECEIVE_TOKEN_LOOP:

			state_ptr->token += 1;

		case EXTERNAL_LOOP:
		case LOOP:

			loops = LOOP_COUNT;
			for(i = 0; i < loops ; i++)
				j = j + i;

			state_ptr->events++;

			delta = LOOKAHEAD + Expent(TAU);
			timestamp = now + TAU + delta;
			ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);

			if (event_type == LOOP)
			{
				if (state_ptr->token)
				{
					delta = LOOKAHEAD + Expent(TAU);
					timestamp = now + TAU + delta;
					ScheduleNewEvent(me, timestamp, PASS_TOKEN_LOOP, NULL, 0);
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
	(void) me;

	if(snapshot->events < COMPLETE_EVENTS)
        return false;

	return true;
}


void write_model_parameters_and_separator(FILE*results_file,char*separator){
	fprintf(results_file,"MODEL_NAME:%s;LOOP:%u;FAN_OUT%u%s",MODEL_NAME,LOOP_COUNT,FAN_OUT,separator);
	//this function is only for statistics purpose
	return;
}