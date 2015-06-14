#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include "core.h"
#include "message_state.h"

#include <lookahead.h>

#define WITH_LOOKAHEAD


static simtime_t *current_time_vector;

static simtime_t *outgoing_time_vector;

static unsigned int *destination_vector;

extern int queue_lock;


void message_state_init(void)
{
  int i;
  
  current_time_vector = malloc(sizeof(simtime_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);
  destination_vector = malloc(sizeof(unsigned int) * n_cores);
  
  for(i = 0; i < n_cores; i++)
  {
    current_time_vector[i] = INFTY;
    outgoing_time_vector[i] = INFTY; //TODO: Oppure a 0 ? riguardare...
    destination_vector[i] = UINT_MAX;
  }
}

void execution_time(simtime_t time, unsigned int id)
{    
  current_time_vector[tid] = time;
  outgoing_time_vector[tid] = INFTY;
  destination_vector[tid] = id;
  
//  if(input_tid != tid && outgoing_time_vector[input_tid] == time)
//    outgoing_time_vector[input_tid] = INFTY;
}

void min_output_time(simtime_t time)
{
  outgoing_time_vector[tid] = time;
}

void commit_time(void)
{
  current_time_vector[tid] = INFTY;
}

int check_safety(simtime_t time, unsigned int *events)
{
  int i;
  unsigned int min_tid = n_cores + 1;
  double min = INFTY;
  int ret = 1;

  *events = 0;

//  while(__sync_lock_test_and_set(&queue_lock, 1))
//    while(queue_lock);
 
  
  for(i = 0; i < n_cores; i++) {
	if(i != tid && destination_vector[i] != destination_vector[tid]  && ((current_time_vector[i] + LOOKAHEAD < min) || (outgoing_time_vector[i] < min)) ) {
	      min = ( current_time_vector[i] + LOOKAHEAD < outgoing_time_vector[i] ?  current_time_vector[i] + LOOKAHEAD : outgoing_time_vector[i]  );
	      min_tid = i;	
	      *events++;
	}
  }

  if(current_time_vector[tid] > min) {
        ret = 0;
        goto out;
  }
 

  for(i = 0; i < n_cores; i++) {
        if(i != tid && destination_vector[i] == destination_vector[tid] && current_time_vector[tid] > current_time_vector[i]) {
		ret = 0;
		goto out;
        }
  }

/* 
 
  for(i = 0; i < n_cores; i++)
  {

    if(i != tid && destination_vector[i] == destination[tid])
	continue;

    if( (i != tid) && ((current_time_vector[i] < min) || (outgoing_time_vector[i] < min)) )
    {
      min = ( current_time_vector[i] < outgoing_time_vector[i] ?  current_time_vector[i] : outgoing_time_vector[i]  );
      min_tid = i;
      *events++;
    }
  }

  if(current_time_vector[tid] < min) {
	ret = 1;
	goto out;
  }

  if(current_time_vector[tid] == min && tid < min_tid) {
       ret = 1;
  }
*/
  
 out:
//  __sync_lock_release(&queue_lock);
  
  return ret;
}


void flush(void) {
  double t_min;
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);

  t_min = queue_deliver_msgs();

  current_time_vector[tid] = INFTY;
  outgoing_time_vector[tid] = t_min;
  destination_vector[tid] = UINT_MAX;

  __sync_lock_release(&queue_lock);
}



