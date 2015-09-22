#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Event types
#define LOOP		1
#define EXTERNAL_LOOP	2

#define LOOP_COUNT 10
#define COMPLETE_EVENTS 10000
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


