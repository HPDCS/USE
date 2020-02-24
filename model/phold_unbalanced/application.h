#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Event types
#define PASS_TOKEN_LOOP		5
#define RECEIVE_TOKEN_LOOP	6
#define EXTERNAL_LOOP		7
#define LOOP				8

#define COMPLETE_EVENTS 5000

#ifndef FAN_OUT
#define FAN_OUT 120
#endif

#ifndef LOOP_COUNT
#define LOOP_COUNT 10
#endif

#ifndef TOKEN_LOOP_COUNT_FACTOR
#define TOKEN_LOOP_COUNT_FACTOR 1000000
#endif

#define TOKEN_LOOP_COUNT (TOKEN_LOOP_COUNT_FACTOR * LOOP_COUNT)

#define TAU 1


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t lvt;
	unsigned int events;
	unsigned int token;
} lp_state_type;