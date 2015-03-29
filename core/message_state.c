#include <stdlib.h>
#include <limits.h>

#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;

static simtime_t *outgoing_time_vector;

extern int queue_lock;


void message_state_init(void)
{
  int i;
  
  current_time_vector = malloc(sizeof(simtime_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);
  
  for(i = 0; i < n_cores; i++)
  {
    current_time_vector[i] = INFTY;
    outgoing_time_vector[i] = INFTY; //TODO: Oppure a 0 ? riguardare...
  }
}

void execution_time(simtime_t time, unsigned int input_tid)
{    
  outgoing_time_vector[tid] = INFTY;
  current_time_vector[tid] = time;
  
  if(input_tid != tid && outgoing_time_vector[input_tid] == time)
    outgoing_time_vector[input_tid] = INFTY;
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
  int i, ret = 1;
  
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);
  
  for(i = 0; i < n_cores; i++)
  {
    if( (i != tid) && ((time >= current_time_vector[i]) || 
	(time >= outgoing_time_vector[i])) )
    {
      if(time == current_time_vector[i] && tid < i)
	ret = 1;
      else
	ret = 0;
      
      break;
    }
  }
  
  __sync_lock_release(&queue_lock);
  
  return ret;
}



