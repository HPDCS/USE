#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>

#define MODEL_NAME "PHOLD_NEARS_RANDOM"

#define COEFF 0.1
#define DIST_BETWEEN_LPS 1.0
// Event types
#define NORMAL_EVT	1
#define ABNORMAL_EVT 2

#define USELESS_INIT_NUMBER 123 //this is initial value of variables i,j but it's useless,because i,j will be overwrited

#ifndef NUM_NEARS //number of neighborhors from me to me+NUM_NEARS mod NUM_LP
#define NUM_NEARS 1
#endif

#define SHIFT 0.001 //must be in (0,1),is useful to generate evts with ts==now+SHIFT,now ts is integer and different from all other now's ts of other LPs when a NORMAL_EVT evt is executed,so now+SHIFT generate ever evts with different ts 

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
	int next_neighbohor;//policy round robin
	unsigned int num_neighbohors_notified;
} lp_state_type;
