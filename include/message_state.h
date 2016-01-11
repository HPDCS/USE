#pragma once
#ifndef __MESSAGESTATE_H
#define __MESSAGESTATE_H


#include "simtypes.h"

/*MAURO, DA ELIMINARE*/
simtime_t get_processing(unsigned int i);

void message_state_init(void);

// It sets the actual execution time in the current thread
void execution_time(simtime_t time, int clp);

unsigned int check_safety(simtime_t time);

unsigned int check_safety_lookahead(simtime_t time);

unsigned int check_safety_no_lookahead(simtime_t time);



#endif
