#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>
#include <preempt_counter.h>
#include "application.h"

void looops(){
	unsigned volatile int i,j = USELESS_INIT_NUMBER;
	unsigned volatile int loops;
	double volatile random_num;
	random_num = Random();
	loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * random_num;
	for(i = 0; i < loops ; i++) {
		j = i*i;
	}
	(void)j;
}

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp_me,timestamp_neighbohor;

	lp_state_type *state_ptr = (lp_state_type*)state;
	(void) me;
	(void) event_content;
	(void) size;

	if(state_ptr != NULL)
		state_ptr->lvt = now;

	switch (event_type) {

		case INIT:
			//TODO check all parameters
            state_ptr = malloc(sizeof(lp_state_type));
			if(state_ptr == NULL) {
				printf("malloc failed\n");
				exit(-1);
			}

			// Explicitly tell ROOT-Sim this is our LP's state
            SetState(state_ptr);
			
			

			if(me == 0) {
				printf("Running %s traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", MODEL_NAME, LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			//TODO print all parameters
			if(NUM_NEARS<=0){
				printf("NUM_NEARS must be greater or equals to 1\n");
				abort();
			}

			timestamp_me =  (simtime_t)((me+1)*DIST_BETWEEN_LPS) ;

			state_ptr->events = 0;
			state_ptr->next_neighbohor=(me+1)%n_prc_tot;
			state_ptr->num_neighbohors_notified=0;

			ScheduleNewEvent(me, timestamp_me, NORMAL_EVT, NULL, 0);
			break;

		case NORMAL_EVT:

			looops();

			state_ptr->events++;//num NORMAL_EVT executed

			timestamp_me= now + (me+1)*Random() + LOOKAHEAD;

			//timestamp_me= now + n_prc_tot +Random() + LOOKAHEAD;

			ScheduleNewEvent(me, timestamp_me , NORMAL_EVT, NULL, 0);//it generates my next evt

			timestamp_neighbohor = now + SHIFT + LOOKAHEAD;

			//commentando questo statement impiega più tempo medio con supporto ipi,anche se effettivamente non fa niente di diverso rispetto alla versione non ipi non posting
			ScheduleNewEvent(state_ptr->next_neighbohor, timestamp_neighbohor, ABNORMAL_EVT, NULL, 0);

			//update_next_neighbohor
			state_ptr->num_neighbohors_notified++;
			if(state_ptr->num_neighbohors_notified == NUM_NEARS){
				state_ptr->num_neighbohors_notified = 0;
				state_ptr->next_neighbohor = (me+ 1)%n_prc_tot;
				#if VERBOSE > 0
				printf("next_neigh=%d,lp_idx=%d\n",state_ptr->next_neighbohor,me);
				#endif
			}
			else{
				state_ptr->next_neighbohor = (state_ptr->next_neighbohor+ 1)%n_prc_tot;//update next_neighbohor
				
				#if VERBOSE > 0
				printf("next_neigh=%d,lp_idx=%d\n",state_ptr->next_neighbohor,me);
				#endif
			}

			break;

		case ABNORMAL_EVT:

			looops();

			break;

		default:
			printf("[ERR] Requested to process an event %u neither ALLOC, nor DEALLOC, nor INIT\n",event_type);
			abort();
			break;
	}
}
	

bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void) me;

	if(snapshot->events < COMPLETE_EVENTS) {
        return false;
    }
	return true;
}