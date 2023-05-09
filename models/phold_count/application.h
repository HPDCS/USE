#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Event types
#define LOOP			8
#define EXTERNAL_LOOP	7

#define VARIANCE 0.2
#define COMPLETE_EVENTS 100000
#define COMPLETE_TIME 200000
#define EVENTS_PER_LP 1
#define FAN_OUT 1

#define LOOP_COUNT 1

#define TAU 1


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t lvt;
	unsigned int events;
	unsigned int num_events;
} lp_state_type;


