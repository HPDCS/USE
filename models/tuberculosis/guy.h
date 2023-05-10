/*
 * guy.h
 *
 *  Created on: 20 lug 2018
 *      Author: andrea
 */

#ifndef MODELS_TUBERCOLOSIS_GUY_H_
#define MODELS_TUBERCOLOSIS_GUY_H_

#include <ROOT-Sim.h>
#include "application.h"



typedef struct _infection_t infection_t;

/* methods for list management */
void insert_in_list(guy_t *guy, region_t *region);
void try_to_insert_guy(guy_t **head, guy_t **tail, guy_t *guy);
guy_t *remove_guy(guy_t **node);


void guy_on_visit(guy_t *guy, unsigned me, region_t *region, simtime_t now);
void guy_on_leave(guy_t *guy, simtime_t now, region_t *region);
void guy_on_infection(infection_t *inf, region_t *region, simtime_t now);


void define_diagnose(guy_t *guy, simtime_t now);
void set_risk_factors(guy_t *guy);
void compute_relapse_p(guy_t* guy, simtime_t now);

#endif /* MODELS_TUBERCOLOSIS_GUY_H_ */
