#include <ROOT-Sim.h>
#include <stdio.h>
#include <limits.h>
#include <lookahead.h>

#include "application.h"


void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, int event_size, lp_state_type *pointer) {

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.new_trails = -1;

	int i, j;
	int receiver, TEMP;
	int trails;

	simtime_t timestamp=0;
	simtime_t delta=0;
	
	for ( i = 0 ; i < 10000 ; i++) 
		j= i*i;

	switch(event_type) {

		case INIT:

			pointer = (lp_state_type *)malloc(sizeof(lp_state_type));
			if(pointer == NULL){
				printf("%s:%d: Unable to allocate memory!\n", __FILE__, __LINE__);
			}
			SetState(pointer);
			pointer->trails;

			if(NUM_CELLE_OCCUPATE > n_prc_tot){
				printf("%s:%d: Require more cell than available LPs\n", __FILE__, __LINE__);
			}

			if((NUM_CELLE_OCCUPATE % 2) == 0){
				//Occupo le "prime" e "ultime" celle
				if(me < (NUM_CELLE_OCCUPATE/2) || me >= ((n_prc_tot)-(NUM_CELLE_OCCUPATE/2))) {
					for(i = 0; i < ROBOT_PER_CELLA; i++) {
						// genero un evento di REGION_IN
						delta = (simtime_t)(20 * Random());
						if(delta < LOOKAHEAD)
							delta += LOOKAHEAD;
						ScheduleNewEvent(me, now + delta, REGION_IN, NULL, 0);
					}
				}
			} else {
				if(me <= (NUM_CELLE_OCCUPATE / 2) || me >= ((n_prc_tot) - (NUM_CELLE_OCCUPATE / 2))){
					for(i = 0; i < ROBOT_PER_CELLA; i++){
						// genero un evento di REGION_IN
						delta = (simtime_t)(20 * Random());
						if(delta < LOOKAHEAD)
							delta += LOOKAHEAD;
						ScheduleNewEvent(me, now + delta, REGION_IN, NULL, 0);
					}
				}
			}


			// Set the values for neighbours. If one is non valid, then set it to -1
			for(i = 0; i < 6; i++) {
				if(isValidNeighbour(me, i)) {
					pointer->neighbour_trails[i] = 0;
				} else {
					pointer->neighbour_trails[i] = -1;
				}
			}

			break;


		case REGION_IN:

			pointer->trails++;

			new_event_content.cell = me;
			new_event_content.new_trails = pointer->trails;
			
			delta = TIME_STEP/100000; //qui voglio indicare che l'evento è immediatamente successivo, ma con il LA questo non succede
					if(delta < LOOKAHEAD)
						delta += LOOKAHEAD;

			for (i = 0; i < 6; i++) {
				if(pointer->neighbour_trails[i] != -1) {
					receiver = GetNeighbourId(me, i);
					if(receiver >= n_prc_tot || receiver < 0)
						printf("%s:%d: %d -> %d\n", __FILE__, __LINE__, me, receiver);						
					
					ScheduleNewEvent(receiver, now + delta, UPDATE_NEIGHBORS, &new_event_content, sizeof(new_event_content));
				}
			}

			// genero un evento di REGION_OUT
			ScheduleNewEvent(me, now + delta, REGION_OUT, NULL, 0);

			break;


		case UPDATE_NEIGHBORS:

			for (i = 0; i < 6; i++) {
				if(event_content->cell == GetNeighbourId(me, i)) {
					pointer->neighbour_trails[i] = event_content->new_trails;
				}
			}

			break;


		case REGION_OUT:

			// Go to the neighbour who has the smallest trails count
			trails = INT_MAX;
			for(i = 0; i < 6; i++) {
				if(pointer->neighbour_trails[i] != -1) {
					if(pointer->neighbour_trails[i] < trails) {
						trails = pointer->neighbour_trails[i];
						receiver = i;
					}
				}
			}
			TEMP = receiver;
			receiver = GetNeighbourId(me, receiver);

			switch (DISTRIBUZIONE) {

				case UNIFORME:
					delta = (simtime_t) (TIME_STEP * Random());
					break;

				case ESPONENZIALE:
					delta = (simtime_t)(Expent(TIME_STEP));
					break;

				default:
					delta = (simtime_t)(TIME_STEP * Random());
		   			break;

			}

			if(receiver >= n_prc_tot || receiver < 0)
				printf("%s:%d: %d -> %d\n", __FILE__, __LINE__, me, receiver);
				
			if (delta < LOOKAHEAD)
				delta += LOOKAHEAD;
			timestamp = now + delta;			
			ScheduleNewEvent(receiver, timestamp, REGION_IN, NULL, 0);
			break;

      		default:
			printf("Error: unsupported event: %d\n", event_type);
			break;
	}
}


// funzione dell'applicazione invocata dalla piattaforma
// per stabilire se la simulazione e' terminata
int OnGVT(unsigned int me, lp_state_type *snapshot) {
	
	printf("value %d ", snapshot->trails);

 	if(snapshot->trails > VISITE_MINIME)
		return true;

	return false;
}
