/**
*
* TRAFFIC is a simulation model for the ROme OpTimistic Simulator (ROOT-Sim)
* which allows to simulate car traffic on generic routes, which can be
* specified from text file.
*
* The software is provided as-is, with no guarantees, and is released under
* the GNU GPL v3 (or higher).
*
* For any information, you can find contact information on my personal webpage:
* http://www.dis.uniroma1.it/~pellegrini
*
* @file application.c
* @brief This module contains events' handler and check termination callbacks
* @author Alessandro Pellegrini
* @date January 12, 2012
*/

#include <ROOT-Sim.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


#include "application.h"
#include "init.h"




void ProcessEvent(unsigned int me, simtime_t now, int event_type, void *event, size_t size, lp_state_type *state) {
	simtime_t timestamp = 0;
	event_content_type new_event;
	int receiver;
	//int i;
	car_t *car;
	
	event_content_type *event_content = (event_content_type *)event;
	
	(void)size;

	if(state != NULL) {
		state->lvt = now;
	}
	
	switch(event_type) {

		// This event initializes the simulation state for each LP and inject first events
		case INIT:
				
			state = malloc(sizeof(lp_state_type));
			if(state == NULL){
				fprintf(stderr, "ERROR: Unable to allocate simulation state!\n");
				fflush(stderr);
				exit(EXIT_FAILURE);
			}

			SetState(state);

			// Initialize state
			bzero(state, sizeof(lp_state_type));

			// Parse the topology file and set state accordingly
			init_my_state(me, state);

			// Set the number of queuable cars in this node
			if(state->lp_type == JUNCTION) {
				state->total_queue_slots = CARS_PER_JUNCTION;
			} else if(state->lp_type == SEGMENT) {
				state->total_queue_slots = (int)(state->segment_length * CARS_PER_UNIT_LENGTH);
			}

			// Set the number of cars in the current node at the beginning of the simulation
			if(state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			}
			
			// Schedule a keep alive event
			timestamp = now + Expent(100);
			ScheduleNewEvent(me, /*LOOKAHEAD +*/ timestamp, KEEP_ALIVE, NULL, 0);
			
			if(me == 0)
				printf("STARTING TRAFFIC WITH END TIME: %u\n input rate: %f\n output prob: %f\n\n",EXECUTION_TIME, state->enter_prob, state->leave_prob);

			// Insert the steady-state number of cars
			//if(state->lp_type == SEGMENT) {
			//	
			//	simtime_t time_slot = (((state->segment_length)*3600)/(AVERAGE_SPEED)); //amount of seconds required to traverse the segment with average speed
			//	//printf("time_slot %f \n", time_slot);
			//	
			//	simtime_t enter_time_interval = 10;//(state->enter_prob/state->leave_prob);
			//	//printf("ep %f - lp %f - enter_time_interval %f \n",state->enter_prob, state->leave_prob, enter_time_interval);
			//	
			//	unsigned int car_in_time_slot = time_slot / enter_time_interval;
			//	
			//	printf("car_in_time_slot %u\n", car_in_time_slot);
			//	
			//	car_t *new_car;
			//	for(unsigned int i = 0; i < car_in_time_slot; i++){
			//		//car = car_enqueue(me, me, state);
			//			new_car = malloc(sizeof(car_t));
			//			bzero(new_car, sizeof(car_t));
			//			new_car->from = me;
			//			new_car->arrival = now - state->enter_prob * (car_in_time_slot - i);
			//			new_car->leave = now /*+ LOOKAHEAD*/ + (i) * 10 + Expent(10);//new_car->arrival + compute_traverse_time(state, AVERAGE_SPEED);
			//			new_car->car_id = get_mark(me, state->car_id++);
			//			//if(state->accident)
			//			//	new_car->accident = true;
			//			state->queued_elements++;
			//
			//			new_car->next = state->queue;
			//			state->queue = new_car;
			//		
			//		// Send a leave event
			//		ScheduleNewEvent(me, new_car->leave, LEAVE, &new_car->car_id, sizeof(unsigned long long));
			//
			//	}
			//}
		
			break;

		case ARRIVAL:

			if(!event_content->injection && check_car_leaving(state, event_content->from, me)) {
				break;
			}
		
			if(state->queued_elements < state->total_queue_slots) {

				car = car_enqueue(me, event_content->from, state);

				// Send a leave event
				ScheduleNewEvent(me, car->leave, LEAVE, &car->car_id, sizeof(unsigned long long));
			} else {
				printf("Object queue full: "); if(state->lp_type == JUNCTION) printf(" JUNCTION\n"); else printf(" ROAD\n");
			}

#if SIMPLE_TRAFFIC == 0		
			cause_accident(state, me);
#endif

			// If the arrival is related to a new car entering the highway, schedule
			// the next car entering
			if(event_content->injection && state->lp_type == JUNCTION) {
				inject_new_cars(state, me);
			}
			
			break;


		case LEAVE: 
			
			car = car_dequeue(me, state, (unsigned long long *)event);
			if(car != NULL) {
				new_event.from = me;
				new_event.injection = false;
				
				if(state->topology->num_neighbours > 1) {
					do {
						receiver = RandomRange(0, state->topology->num_neighbours - 1);
						receiver = state->topology->neighbours[receiver];
					} while(receiver == car->from);				
				} else {
					receiver = state->topology->neighbours[0];
				}
				
				ScheduleNewEvent(receiver, /*LOOKAHEAD +*/ car->leave, ARRIVAL, &new_event, sizeof(event_content_type));
				free(car);
			} else {
				timestamp = now + Expent(ACCIDENT_LEAVE_TIME);
				update_car_leave(state, *(unsigned long long *)event_content, timestamp);
				ScheduleNewEvent(me, /*LOOKAHEAD +*/ timestamp, LEAVE, (unsigned long long *)event_content, sizeof(unsigned long long));
			}
			
			break;
			

		case FINISH_ACCIDENT:
			state->accident = false;
			release_cars(me, state);
			break;


		case KEEP_ALIVE:
#if SIMPLE_TRAFFIC == 0		
			determine_stop(state);
#endif
			timestamp = now + Expent(100);
			ScheduleNewEvent(me, /*LOOKAHEAD +*/ timestamp, KEEP_ALIVE, NULL, 0);
			break;

      		default:
			printf(" state simulation: error - inconsistent event (me = %d - event type = %d)\n",me,event_type);
			break;
	}
}



bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void)me;
	
	//~if(snapshot->accident)
		//~printf("Node %s is in accident state\n", snapshot->name);
	
	if (snapshot->lvt < EXECUTION_TIME)
		return false;
	//printf("LP %u has finished at time %f\n", me, snapshot->lvt);
	return true;
}
