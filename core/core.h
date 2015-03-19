#ifndef __CORE_H
#define __CORE_H


#include "event_type.h"
#include "time_util.h"


typedef struct __event_commit_synchronization
{
  // Timestamp dell'ultimo evento generato
  timestamp_t last_timestamp_activated;
  // Timestamp dell'ultimo evento committato
  timestamp_t minimum_timestamp_committed;

} event_commit_synchronization;


void init(void);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);


#endif
