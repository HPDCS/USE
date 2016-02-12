#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>

#include "application.h"
#include "functions.h"

//#include <timer.h>

//lp_state_type states[4] __attribute__((aligned (64)));

static unsigned int load[4] = {2, 4, 5, 1};

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp, delta;
	unsigned int i, j;
	int err;
	unsigned int loops; 
	lp_state_type *state_ptr;

	unsigned long long current_hash;

	//printf("LP%d event at %.3f\n", me, now);

	switch (event_type) {

		case INIT:

			if(me == 0) {
				printf("Running HASH benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			
			//Allocate a pointer of 64 bytes aligned to 64 bytes (cache line size)
			err = posix_memalign((void **)&state_ptr, 64, sizeof(lp_state_type));
			if(err != 0) {
				printf("posix_memalign failed! (%d)\n", err);
				exit(-1);
			}

			/*if(state_ptr == NULL){
				printf("LP state allocation failed: (%s)\n", strerror(errno));
				exit(-1);
			}*/
			memset(state_ptr, 0, sizeof(lp_state_type));

			state_ptr->hash = hash((void *) state_ptr, (sizeof(lp_state_type) - sizeof(state_ptr->hash)));

			// Explicitly tell ROOT-Sim this is our LP's state
			SetState(state_ptr);
			
			// Each INIT event will schedule a number of events
			// to simulate a higher system load
			for(i = 0; i < 10; i++) {
				timestamp = (simtime_t) (20 * Random());
				if(timestamp < LOOKAHEAD)
					timestamp += LOOKAHEAD;

				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}

			break;


		case EXTERNAL_LOOP:
		case LOOP:

			// Load the state pointer
			state_ptr = state;
			//printf("state at: %p\n", state);

			// Check the hash of the event's state if matches with the current one
			current_hash = hash((void *) state_ptr, (sizeof(lp_state_type) - sizeof(unsigned long long)));

			//state_dump(me, state_ptr, sizeof(lp_state_type));

			if (current_hash != state_ptr->hash) {
				printf("[LP%d] Possible state corruption at time %.3f: state hash does not match! (%llx -> %llx)\n", me, now, state_ptr->hash, current_hash);
				//exit(-1);
			}

			// Once the state has been checked we can proceed to modify the actual state
			// with the other information computed by this event execution

			// Update LP's states with the number of the total events
			// and its actual hash value (checked at the following processing)
			state_ptr->events++;
			state_ptr->current_lvt = now;

			// Simulate a random memory access pattern
			unsigned int span = SPAN_BASE + (SPAN_VAR * (2 * Random() - 1));
			for (i = 0; i < span; i++) {
				for (j = 0; j < load[state_ptr->epoch]; j++) {
					// Read and write on some cells of the data region
					state_ptr->data[(unsigned int)(DATA_SIZE * Random())] += (i*i);
				}
			}

			// Busy loop, just do nothing...
			//loops = LOOP_COUNT * 29 * (1 - VARIANCE) + 2 * (LOOP_COUNT * 29) * VARIANCE * Random();
			loops = (LOOP_COUNT * 10) + ((LOOP_COUNT_VAR * 10) * (Random() - 1));		// loops e [0, LOOP_COUNT]
			for(i = 0; i < loops ; i++) {
					state_ptr->counter = i*i;
			}

			if(state_ptr != 0 && (state_ptr->events % (COMPLETE_EVENTS / 4) == 0)) {
				state_ptr->epoch++;
			}

			// The very LAST thing to do is to update LP'state hash
			state_ptr->hash = hash((void *) state_ptr, (sizeof(lp_state_type) - sizeof(state_ptr->hash)));


			// Compute the timestamp of the next event to schedule
			delta = (simtime_t)(Expent(TAU));
			if(delta < LOOKAHEAD)
				delta += LOOKAHEAD;
			timestamp = now + delta;

			// Schedule the next event to the same LP
			if(event_type == LOOP) {
				ScheduleNewEvent(me, timestamp, LOOP, NULL, 0);
			}

			// With a small probability the next event will be scheduled for another LP
			if(event_type == LOOP && Random() < 0.2) {
			//	ScheduleNewEvent(FindReceiver(TOPOLOGY_MESH), timestamp, EXTERNAL_LOOP, NULL, 0);
			}
			break;


		default:
			printf("[ERR] Requested to process an unknown event type\n");
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {

	if(snapshot->events < COMPLETE_EVENTS) {
//	if(snapshot->lvt < COMPLETE_TIME) {
		return false;
	}
	return true;
}

