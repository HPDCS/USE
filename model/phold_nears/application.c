#include <stdlib.h>
#include <stdio.h>
#include <ROOT-Sim.h>
#include <errno.h>

#include <lookahead.h>
#include <preempt_counter.h>
#include "application.h"

void ProcessEvent(int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *state) {

	simtime_t timestamp;
	unsigned volatile int i,j = USELESS_INIT_NUMBER;
	unsigned volatile int loops;
	double volatile random_num;

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
			
			state_ptr->events = 0;

			if(me == 0) {
				printf("Running %s a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", MODEL_NAME,LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			//TODO print all parameters
			if(NUM_NEARS<=0){
				printf("NUM_NEARS must be greater or equals to 1\n");
				abort();
			}
			timestamp =  (simtime_t)(me) ;// ts normal_evt==i+NUM_LP,ts is integer
			state_ptr->next_neighbohor=(me+1)%n_prc_tot;
			state_ptr->num_neighbohors_notified=0;
			ScheduleNewEvent(me, timestamp, NORMAL_EVT, NULL, 0);
			break;

		case NORMAL_EVT:

			//TODO debug ts is integer
			random_num = Random();
			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * random_num;
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			state_ptr->events++;//num NORMAL_EVT executed

			ScheduleNewEvent(me, now+n_prc_tot+LOOKAHEAD, NORMAL_EVT, NULL, 0);//it generates my next evt

			timestamp= now + SHIFT+LOOKAHEAD;

			ScheduleNewEvent(state_ptr->next_neighbohor, timestamp, ABNORMAL_EVT, NULL, 0);

			state_ptr->num_neighbohors_notified++;
			if(state_ptr->num_neighbohors_notified == NUM_NEARS){
				state_ptr->num_neighbohors_notified = 0;
				state_ptr->next_neighbohor = (me+ 1)%n_prc_tot;
			}
			else{
				state_ptr->next_neighbohor = (state_ptr->next_neighbohor+ 1)%n_prc_tot;//update next_neighbohor
			}

			break;

		case ABNORMAL_EVT:

			//TODO debug ts-SHIFT is integer
			random_num= Random();
			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * random_num;
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			break;

		default:
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
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

void write_model_parameters_and_separator(FILE*results_file,char*separator){
	fprintf(results_file,"MODEL_NAME:%s;LOOP:%u;NUM_NEARS:%u%s",MODEL_NAME,LOOP_COUNT,NUM_NEARS,separator);
	//this function is only for statistics purpose
	return;
}