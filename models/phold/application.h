#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>
#include <clock_constant.h>
#include <timer.h>

// Event types
#define LOOP			8
#define EXTERNAL_LOOP	7

#define TAU 1
#define EVENTS_PER_LP 5


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t lvt;
	unsigned int events;
	unsigned int event_us;
	unsigned int fan_out;
	unsigned int num_events;
} lp_state_type __attribute__((aligned(64)));


