#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ROOT-Sim.h>

#include "application.h"

bool pcs_statistics = false;
int complete_calls = COMPLETE_CALLS;


#define DUMMY_TA 500


void ProcessEvent(unsigned int me, simtime_t now, unsigned int event_type, event_content_type *event_content, unsigned int size, void *ptr) {

	int w;

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.channel = -1;
	new_event_content.call_term_time = -1;

	simtime_t handoff_time;
	simtime_t timestamp = 0;

	lp_state_type *state;
	state = (lp_state_type*)ptr;

//	printf("Executing %d at %d at %f state %p\n", event_type, me, now, ptr);

	if(state != NULL) {
		state->lvt = now;
		state->executed_events++;
	}

	unsigned char *p[CHANNELS_PER_CELL];


	switch(event_type) {

		case INIT:

			// Initialize the LP's state
			state = (lp_state_type *)malloc(sizeof(lp_state_type));
			if (state == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}

			SetState(state);

			bzero(state, sizeof(lp_state_type));
			state->channel_counter = CHANNELS_PER_CELL;

			state->ref_ta = state->ta = TA;
			state->ta_duration = TA_DURATION;
			state->ta_change = TA_CHANGE;
			state->channels_per_cell = CHANNELS_PER_CELL;

			for(w = 0; w < CHANNELS_PER_CELL; w++) {
				p[w] = malloc(sizeof(channel));
				*p[w] = 'x';
			}
			for(w = 0; w < CHANNELS_PER_CELL; w++) {
				free(p[w]);
			}
			for(w = 0; w < CHANNELS_PER_CELL; w++) {
				p[w] = malloc(sizeof(sir_data_per_cell));
				*p[w] = 'x';
			}
			for(w = 0; w < CHANNELS_PER_CELL; w++) {
				free(p[w]);
			}


			// Show current configuration, only once
			if(me == 0) {
				printf("CURRENT CONFIGURATION:\ncomplete calls: %d\nTA: %f\nta_duration: %f\nta_change: %f\nchannels_per_cell: %d\nfading_recheck: %d\nvariable_ta: %d\n",
					complete_calls, state->ta, state->ta_duration, state->ta_change, state->channels_per_cell, state->fading_recheck, state->variable_ta);
				fflush(stdout);
			}

			state->channel_counter = state->channels_per_cell;

			// Setup channel state
			state->channel_state = malloc(sizeof(unsigned int) * 2 * (CHANNELS_PER_CELL / BITS + 1));
			for (w = 0; w < state->channel_counter / (sizeof(int) * 8) + 1; w++)
				state->channel_state[w] = 0;

			// Start the simulation
			timestamp = (simtime_t) (20 * Random());
			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);
//			printf("INIT %d: START at %f\n", me, timestamp);

			// If needed, start the first fading recheck
//			if (state->fading_recheck) {
//				timestamp = (simtime_t) (FADING_RECHECK_FREQUENCY * Random());
//				ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
//			}
//			printf("INIT %d: RECHECK at %f\n", me, timestamp);

			break;


		case START_CALL:

			state->arriving_calls++;

			if (state->channel_counter == 0) {
				state->blocked_on_setup++;
			} else {

				state->channel_counter--;

//				new_event_content.channel = allocation(state);
				new_event_content.from = me;
				new_event_content.sent_at = now;

				// Determine call duration
				switch (DURATION_DISTRIBUTION) {

					case UNIFORM:
						new_event_content.call_term_time = now + (simtime_t)(state->ta_duration * Random());
						break;

					case EXPONENTIAL:
						new_event_content.call_term_time = now + (simtime_t)(Expent(state->ta_duration));
						break;

					default:
 						new_event_content.call_term_time = now + (simtime_t) (5 * Random() );
				}

				// Determine whether the call will be handed-off or not
				switch (CELL_CHANGE_DISTRIBUTION) {

					case UNIFORM:

						handoff_time  = now + (simtime_t)((state->ta_change) * Random());
						break;

					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent(state->ta_change));
						break;

					default:
						handoff_time = now + (simtime_t)(5 * Random());

				}

				if(new_event_content.call_term_time <= handoff_time) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(TOPOLOGY_HEXAGON);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
					ScheduleNewEvent(new_event_content.cell, handoff_time, HANDOFF_RECV, &new_event_content, sizeof(new_event_content));
				}
			}


			if (state->variable_ta)
				state->ta = recompute_ta(state->ref_ta, now);

			// Determine the time at which a new call will be issued
			switch (DISTRIBUTION) {

				case UNIFORM:
					timestamp= now + (simtime_t)(state->ta * Random());
					break;

				case EXPONENTIAL:
					timestamp= now + (simtime_t)(Expent(state->ta));
					break;

				default:
					timestamp= now + (simtime_t) (5 * Random());

			}

			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			break;

		case END_CALL:

			state->channel_counter++;
			state->complete_calls++;
//			deallocation(me, state, event_content->channel, event_content, now);

			break;

		case HANDOFF_LEAVE:

			state->channel_counter++;
			state->leaving_handoffs++;
//			deallocation(me, state, event_content->channel, event_content, now);

			new_event_content.call_term_time =  event_content->call_term_time;
			break;

        	case HANDOFF_RECV:
			state->arriving_handoffs++;
			state->arriving_calls++;

			if (state->channel_counter == 0)
				state->blocked_on_handoff++;
			else {
				state->channel_counter--;

//				new_event_content.channel = allocation(state);
				new_event_content.call_term_time = event_content->call_term_time;

				switch (CELL_CHANGE_DISTRIBUTION) {
					case UNIFORM:
						handoff_time  = now + (simtime_t)((state->ta_change) * Random());

						break;
					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent( state->ta_change ));

						break;
					default:
						handoff_time = now+
			    			(simtime_t) (5 * Random());
				}

				if(new_event_content.call_term_time < handoff_time ) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(TOPOLOGY_HEXAGON);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
					ScheduleNewEvent(new_event_content.cell, handoff_time, HANDOFF_RECV, &new_event_content, sizeof(new_event_content));
				}
			}


			break;


		case FADING_RECHECK:

/*
			if(state->check_fading)
				state->check_fading = false;
			else
				state->check_fading = true;
*/

			fading_recheck(state);

			timestamp = now + (simtime_t) (FADING_RECHECK_FREQUENCY );
			ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);

			break;


		default:
			fprintf(stdout, "PCS: Unknown event type! (me = %d - event type = %d)\n", me, event_type);
			abort();

	}
}


bool OnGVT(unsigned int me, lp_state_type *snapshot) {

	if (snapshot->complete_calls < complete_calls)
		return false;
	return true;
}
