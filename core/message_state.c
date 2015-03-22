#include <stdlib.h>
#include <float.h>

#include "core.h"
#include "message_state.h"



static timestamp_t *current_time_vector;

static timestamp_t *outgoing_time_vector;

static int lock = 0;


void message_state_init(void)
{
  int i;
  
  current_time_vector = malloc(sizeof(timestamp_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(timestamp_t) * n_cores);
  
  for(i = 0; i < n_cores; i++)
  {
    current_time_vector[i] = DBL_MAX;
    outgoing_time_vector[i] = DBL_MAX; //TODO: Oppure a 0 ? riguardare...
  }
}

void execution_time(timestamp_t time)
{
  while(__sync_lock_test_and_set(&lock, 1))
    while(lock);
    
  outgoing_time_vector[tid] = DBL_MAX;
  current_time_vector[tid] = time;
    
  __sync_lock_release(&lock);
}

void min_output_time(timestamp_t time)
{
  outgoing_time_vector[tid] = time;
}

void commit_time(void)
{
  current_time_vector[tid] = DBL_MAX;
}

int check_safety(timestamp_t time)
{
  int i;
  
  while(__sync_lock_test_and_set(&lock, 1))
    while(lock);
  
  for(i = 0; i < n_cores; i++)
  {
    if( (i != tid) && (time > current_time_vector[i]) && 
	(time > outgoing_time_vector[i]) )
      return 0;
  }
  
  __sync_lock_release(&lock);
  
  return 1;
}



