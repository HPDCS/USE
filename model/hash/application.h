#include <ROOT-Sim.h>
#include <math.h>
#include <stdlib.h>


// Event types
#define LOOP		1
#define EXTERNAL_LOOP	2

#define VARIANCE 0.2
#define LOOP_COUNT 100
#define LOOP_COUNT_VAR 0
#define COMPLETE_EVENTS 10000
#define COMPLETE_TIME 200000
#define TAU 100

#define SPAN_BASE 100		// Define the average number of memory cell accessed during processing
#define SPAN_VAR 0			// Define the variance around the previous average

#define DATA_SIZE 1024		// The number of cells of data in the event's state


// This is the events' payload which is exchanged across LPs
typedef struct _event_content_type {
	int size;
} event_content_type;


// LP simulation state
typedef struct _lp_state_type {
	simtime_t current_lvt;				// Keep track of the event's timestamp at the time it will be processed in future
	unsigned int events;				// The total number of events being processed so far by the current LP
	unsigned char data[DATA_SIZE];		// Array of a generic data to simulate meemory operations
	unsigned long long hash;			// LP's state hash (MUST BE the LAST field)
} lp_state_type;


