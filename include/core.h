#ifndef __CORE_H
#define __CORE_H

#include <stdbool.h>

#include "time_util.h"


#define MAX_LPs	2048


void init(void);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);

extern __thread timestamp_t current_lvt;
extern __thread unsigned int current_lp;

extern void rootsim_error(bool fatal, const char *msg, ...);


#endif
