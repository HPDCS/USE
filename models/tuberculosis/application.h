/*
 * application.h
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_APPLICATION_H_
#define MODELS_TUBERCOLOSIS_APPLICATION_H_

#include <ROOT-Sim.h>
#include "t_bitmap.h"


// the flags used in the guy struct
enum _flags_t{
	f_sick,		/// the guy is either in the "sick" state or the "under treatment" state IFF this flag is set
	f_treatment,	/// the guy is either in the "treated" state or the "under treatment" state IFF this flag is set
	f_foreigner,
	f_female,
	f_smear,
	f_smokes,
	f_has_diabetes,
	f_has_hiv,
	flags_count
};



// notice that we use absolute times for guys fields so that we don't need to refresh all those memory location
// at each time step with only a minor increase in operations (some integer additions)
// TODO the guy struct can be even more space efficient with some tricks: implement them
typedef struct _guy_t{
	rootsim_bitmap flags[bitmap_required_size(flags_count)];
	int birth_day; /// this is an int since a guy could be born in the past (before the simulation start)
	int infection_day;
	int treatment_day;
	double p_relapse;
	struct _guy_t *prev,*next;
} guy_t;


#define RegionsCount() (pdes_config.nprocesses)

typedef struct _region_t{
	unsigned healthy;
	unsigned guys_infected;

  /* init parameters */
	unsigned num_healthy;
    unsigned num_infected;
	unsigned num_sick;
	unsigned num_treatment;
	unsigned num_treated;

  /* head/tail of guy's lists */
	guy_t *head_infected, *head_sick, *head_treatment, *head_treated;
	guy_t *tail_infected, *tail_sick, *tail_treatment, *tail_treated;
	
	simtime_t now;
	void *sick,*infected,*treated,*treatment;
} region_t;

enum _event_t{
	MIDNIGHT = INIT + 1,
	RECEIVE_HEALTHY,
	INFECTION,
	GUY_VISIT,
	GUY_LEAVE,
	GUY_INIT,
	_TRAVERSE
};
// this samples a random number with binomial distribution TODO could be useful in the numeric library
unsigned random_binomial(unsigned trials, double p);

#endif /* MODELS_TUBERCOLOSIS_APPLICATION_H_ */
