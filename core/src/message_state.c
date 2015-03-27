#include <stdlib.h>

#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;

static simtime_t *outgoing_time_vector;


extern int comm_lock;


void message_state_init(void)
{
  int i;
  
  current_time_vector = malloc(sizeof(simtime_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);
  
  for(i = 0; i < n_cores; i++)
  {
    current_time_vector[i] = INFTY;
    outgoing_time_vector[i] = INFTY; 
  }
}

void execution_time(simtime_t time, unsigned int generator_tid)
{    
  outgoing_time_vector[tid] = INFTY;
  current_time_vector[tid] = time;
  
  if(generator_tid != tid && outgoing_time_vector[generator_tid] == time)
    outgoing_time_vector[generator_tid] = INFTY;
}

void min_output_time(simtime_t time)
{
  outgoing_time_vector[tid] = time;
}

void commit_time(void)
{
  current_time_vector[tid] = INFTY;
}

int check_safety(simtime_t time)
{
  int i;
  
  while(__sync_lock_test_and_set(&comm_lock, 1))
    while(comm_lock);
  
  for(i = 0; i < n_cores; i++)
  {
    if( (i != tid) && ((time > current_time_vector[i]) || 
	(time > outgoing_time_vector[i])) )
    {
      __sync_lock_release(&comm_lock);
      return 0;
    }
  }
  
  __sync_lock_release(&comm_lock);
  
  return 1;
}




