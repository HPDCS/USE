#pragma once
#ifndef _TCAR_H
#define _TCAR_H


#include <ROOT-Sim.h>

/* TIMESTAMP DISTRIBUTION */
#define UNIFORME	0
#define ESPONENZIALE 	1

#define DISTRIBUZIONE ESPONENZIALE


#define TIME_STEP 1.0


// EVENT TYPES
#define REGION_IN	1
#define REGION_OUT	2
#define UPDATE_NEIGHBORS	3


#ifndef NUM_CELLE_OCCUPATE
	#define NUM_CELLE_OCCUPATE	80
#endif
#ifndef ROBOT_PER_CELLA
	#define ROBOT_PER_CELLA 2
#endif

#ifndef VISITE_MINIME
	#define VISITE_MINIME	1000
#endif

#ifndef LOOP_COUNT
	#define LOOP_COUNT	1000
#endif


typedef struct _event_content_type {
	int cell;
	int new_trails;
} event_content_type;




typedef struct _lp_state_type{
	int trails;
	int neighbour_trails[6];
} lp_state_type;


int FindNeighbour(unsigned int sender);
bool isValidNeighbour(unsigned int sender, unsigned int neighbour);
int GetNeighbourId(unsigned int sender, unsigned int neighbour);


#endif /* _ANT_ROBOT_H */
