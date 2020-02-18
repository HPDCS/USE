#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>

// Event types
#define NORMAL_EVT	1
#define ABNORMAL_EVT 2

#define USELESS_INIT_NUMBER 123 //this is initial value of variables i,j but it's useless,because i,j will be overwrited

#ifndef THR_PROB_NORMAL //threshold prob to gossip to another LP
#define THR_PROB_NORMAL 1.0//greater is heaviest
#endif

#ifndef THR_PROB_ABNORMAL //threshold prob to gossip to another LP
#define THR_PROB_ABNORMAL 0.0//greater is heaviest
#endif

#define SHIFT 0.5 //must be in (0,1),is useful to generate evts with ts==now+SHIFT,now ts is integer and different from all other now's ts of other LPs when a NORMAL_EVT evt is executed,so now+SHIFT generate ever evts with different ts 

#ifndef VARIANCE
#define VARIANCE 0.2
#endif

#define COMPLETE_EVENTS 5000

#ifndef LOOP_COUNT
#define LOOP_COUNT 100
#endif

#define MIN_LOOPS (LOOP_COUNT * 29 * (1 - VARIANCE))
#define COEFFICIENT_OF_RANDOM (2 * (LOOP_COUNT * 29) * VARIANCE)

// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t lvt;
	unsigned int events;
} lp_state_type;


