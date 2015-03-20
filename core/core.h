#ifndef __CORE_H
#define __CORE_H


#include "event_type.h"
#include "calqueue.h"
#include "time_util.h"


void init(void);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);


#endif
