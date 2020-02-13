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
			#if HANDLE_INTERRUPT==1
			// increment_preempt_counter();
			#endif
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
				printf("Running a traditional loop-based PHOLD benchmark with counter set to %d, %d total events per LP, lookahead %f\n", LOOP_COUNT, COMPLETE_EVENTS, LOOKAHEAD);
			}
			//TODO print all parameters

			timestamp =  (simtime_t)(me + n_prc_tot) ;// ts normal_evt==i+NUM_LP,ts is integer
			ScheduleNewEvent(me, timestamp, NORMAL_EVT, NULL, 0);
			#if HANDLE_INTERRUPT==1
			// decrement_preempt_counter();
			#endif
			break;

		case NORMAL_EVT:

			//TODO debug ts is integer
			#if HANDLE_INTERRUPT==1
			increment_preempt_counter();
			#endif
			random_num = Random();
			#if HANDLE_INTERRUPT==1
			decrement_preempt_counter();
			#endif
			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * random_num;
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			state_ptr->events++;//num NORMAL_EVT executed

			#if HANDLE_INTERRUPT==1
			increment_preempt_counter();
			#endif

			ScheduleNewEvent(me, now+n_prc_tot, NORMAL_EVT, NULL, 0);//it generates my next evt
			
			#if HANDLE_INTERRUPT==1
			decrement_preempt_counter();
			#endif

			for(i = 0; i < n_prc_tot; i++){
				#if HANDLE_INTERRUPT==1
				increment_preempt_counter();
				#endif
				random_num=Random();
				#if HANDLE_INTERRUPT==1
				decrement_preempt_counter();
				#endif
				if(i!=(unsigned int)me && THR_PROB_NORMAL >= random_num ){
					timestamp=(simtime_t)(now+SHIFT);
					#if HANDLE_INTERRUPT==1
					increment_preempt_counter();
					#endif
					ScheduleNewEvent(i, timestamp, ABNORMAL_EVT, NULL, 0);
					#if HANDLE_INTERRUPT==1
					decrement_preempt_counter();
					#endif
				}
			}

			break;

		case ABNORMAL_EVT:

			//TODO debug ts-SHIFT is integer
			#if HANDLE_INTERRUPT==1
			increment_preempt_counter();
			#endif
			random_num= Random();
			#if HANDLE_INTERRUPT==1
			decrement_preempt_counter();
			#endif
			loops= MIN_LOOPS + COEFFICIENT_OF_RANDOM * random_num;
			for(i = 0; i < loops ; i++) {
				j = i*i;
			}
			(void)j;

			break;

		default:
			#if HANDLE_INTERRUPT==1
			increment_preempt_counter();
			#endif
			printf("[ERR] Requested to process an event neither ALLOC, nor DEALLOC, nor INIT\n");
			abort();
			#if HANDLE_INTERRUPT==1
			decrement_preempt_counter();
			#endif
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

