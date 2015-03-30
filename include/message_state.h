#pragma once
#ifndef __MESSAGESTATE_H
#define __MESSAGESTATE_H


#include "simtypes.h"


void message_state_init(void);

// It sets the actual execution time in the current thread
void execution_time(simtime_t time);

// It sets the minimum outgoing time from the current thread
void min_output_time(simtime_t time);

// Commit the actual execution time in the current thread
void commit_time(void);

// It returns 1 if there is not any other timestamp less than "time" in the timestamps executed and in all outgoing messages.
int check_safety(simtime_t time, unsigned int *);


#endif
