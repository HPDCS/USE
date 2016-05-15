#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Event types
#define LOOP		8
#define EXTERNAL_LOOP	7

#define VARIANCE 0.2
#define LOOP_COUNT 100
#define COMPLETE_EVENTS 1000
#define COMPLETE_TIME 200000
#define TAU 100


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t lvt;
	unsigned int events;
} lp_state_type;


