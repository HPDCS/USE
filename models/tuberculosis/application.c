/*
 * application.c
 *
 *  Created on: 19 lug 2018
 *      Author: andrea
 */

#include "application.h"
#include "guy.h"
#include "guy_init.h"
#include <ROOT-Sim.h>


#include <math.h>


typedef struct model_parameters{
	unsigned num_healthy;
  unsigned num_infected;
  unsigned num_sick;
  unsigned num_treatment;
  unsigned num_treated;
  simtime_t end_sim;
}
model_parameters;

struct argp_option model_options[] = {
	{"num_healthy",        1001, "VALUE", 0, "Number of healthy people"               , 0 },
	{"num_infected",        1002, "VALUE", 0, "Number of infected people"               , 0 },
	{"num_sick",        1003, "VALUE", 0, "Number of sick people"               , 0 },
	{"num_treatment",        1004, "VALUE", 0, "Number of people in treatment"               , 0 },
	{"num_treated",        1005, "VALUE", 0, "Number of treated people"               , 0 },
	{"end_sim",        1006, "VALUE", 0, "Simulation end time"               , 0 },
  { 0, 0, 0, 0, 0, 0} 
};

model_parameters args = {
	.num_healthy = 1529469,
  .num_infected = 68491,
  .num_sick = 39,
  .num_treatment = 161,
  .num_treated = 1840,
  .end_sim	   = 10.0,
};



error_t model_parse_opt(int key, char *arg, struct argp_state *state){

	(void)state;
	switch(key){
		case 1001:
			args.num_healthy = atoi(arg);
			break;
		case 1002:
			args.num_infected = atoi(arg);
			break;
		case 1003:
			args.num_sick = atoi(arg);
			break;
		case 1004:
			args.num_treatment = atoi(arg);
			break;
		case 1005:
			args.num_treated = atoi(arg);
			break;
		case 1006:
			args.end_sim = strtod(arg, NULL);
			break;
		default:
			break;
	}
	return 0;
}


// From Luc Devroye's book "Non-Uniform Random Variate Generation." p. 522
unsigned random_binomial(unsigned trials, double p) { // this is exposed since it is used also in guy.c
	if(p >= 1.0 || !trials)
		return trials;
	unsigned x = 0;
	double sum = 0, log_q = log(1.0 - p); // todo cache those logarithm value
	while(1) {
		sum += log(Random()) / (trials - x);
		if(sum < log_q || trials == x) {
			return x;
		}
		x++;
	}
	return 0;
}

// this is better than calling FindReceiver() for each healthy person
static void move_healthy_people(unsigned me, region_t *region, simtime_t now){
	(void)me;
	// these are the theoretical neighbours (that is, NeighboursCount ignores borders)
	const unsigned neighbours = GetDirectionCount(TOPOLOGY_SQUARE);
	unsigned i = neighbours, to_send;
	// we use this to keep track of the already dispatched neighbours
	rootsim_bitmap explored[bitmap_required_size(neighbours)];
	bitmap_initialize(explored, neighbours);
	// now we count the actual neighbours TODO this is probably a common operation: APIFY IT!
	unsigned actual_neighbours = 0;
	while(i--){
		if(GetNeighbourFromDirection(TOPOLOGY_SQUARE, i) != UNDEFINED_LP)
			actual_neighbours++;
		else
			bitmap_set(explored, i); // this way we won't ask for that direction again
	}
	// we do this in 2 rounds to minimize the effect of the bias of the binomial generator xxx: is it needed? Perform some tests!
	while(actual_neighbours){
		// we pick a new random direction
		i = Random()*neighbours;
		if(bitmap_check(explored, i))
			continue;
		// we mark this direction to "explored"
		bitmap_set(explored, i);
		// we compute the number of healthy people who choose this direction
		to_send = random_binomial(region->healthy, 1.0/actual_neighbours);
		// we send the actual event
		ScheduleNewEvent(GetNeighbourFromDirection(TOPOLOGY_SQUARE, i), now, RECEIVE_HEALTHY, &to_send, sizeof(unsigned));
		// those people just left this region
		region->healthy -= to_send;
		// we processed a valid neighbour so we decrease the counter
		actual_neighbours--;
	}
}

// we handle infects visits move at slightly randomized timesteps 1.0, 2.0, 3.0...
// healthy people are moved at slightly randomized timesteps 0.5, 1.5, 2.5, 3.5...
// this way we preserve the order of operation as in the original model
void ProcessEvent(unsigned int me, simtime_t now, int event_type,
		union {guy_t *guy; unsigned *n; infection_t *i_m; init_t *in_m;} payload, unsigned int event_size, region_t *state) {

	if(!me && event_type != INIT)
		state->now = now;

	switch (event_type) {
		case INIT:
			(void)event_size;
			// standard stuff
			region_t *region = malloc(sizeof(region_t));
			SetState(region);
			region->healthy = 0;
			region->guys_infected = 0;
			/// parameters initialization
			region->num_healthy		= args.num_healthy;
			region->num_sick			= args.num_sick;
			region->num_infected	= args.num_infected;
			region->num_treatment	= args.num_treatment;
			region->num_treated		= args.num_treated;
			region->end_sim			= args.end_sim;

			/// lists head/tail initialization
			region->head_infected = region->tail_infected = NULL;
			region->head_sick = region->tail_sick =  NULL;
			region->head_treatment = region->tail_treatment = NULL;
			region->head_treated = region->tail_treated = NULL;


			if(!me){
				// this function let LP0 coordinate the init phase
				guy_init(region);
				printf("INIT 0 complete\n");
				printf("Model configuration: \n");
				printf("- Healthy guys: %u\n", region->num_healthy);
				printf("- Sick guys: %u\n", region->num_sick);
				printf("- Infected guys: %u\n", region->num_infected);
				printf("- Treatment guys: %u\n", region->num_treatment);
				printf("- Treated guys: %u\n", region->num_treated);
				printf("- End simulation time: %u\n", region->end_sim);
			}
			ScheduleNewEvent(me, now + 1.25 + Random()/2, MIDNIGHT, NULL, 0);
			break;

		case MIDNIGHT:
			move_healthy_people(me, state, now);
			ScheduleNewEvent(me, now + 0.25 + Random()/2, MIDNIGHT, NULL, 0);
			break;

		case RECEIVE_HEALTHY:
			state->healthy += *(payload.n);
			break;

		case INFECTION:
			guy_on_infection(payload.i_m, state, now);
			break;

		case GUY_VISIT:
			guy_on_visit(payload.guy, me, state, now);
			break;

		case GUY_LEAVE:
			guy_on_leave(payload.guy, now, state);
			break;

		case GUY_INIT:
			guy_on_init(payload.in_m, state);
			break;

		case _TRAVERSE:
		default:
			printf("%s:%d: Unsupported event: %d\n", __FILE__, __LINE__, event_type);
			exit(EXIT_FAILURE);
	}

}

int OnGVT(unsigned int me, region_t *snapshot) {
	if(!me){
		//printf("healthy %u -- infected %u\n", snapshot->healthy, snapshot->guys_infected);
		return snapshot->now > snapshot->end_sim;
	}
	return true;
}
