#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>

#include "core.h"
#include "calqueue.h"
#include "message_state.h"
#include "communication.h"
#include "simtypes.h"

#include "time_util.h"

//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127



#ifdef FINE_GRAIN_DEBUG

static int __tt_lock = 0;
static simtime_t __tt_comm = 0;

#define CHECKER do{					\
    while(__sync_lock_test_and_set(&__tt_lock, 1))	\
      while(__tt_lock);					\
    if(__tt_comm > current_lvt)				\
    {							\
      printf("simulation error  -----------------------------------------------------------------------------------------------------------------------------------\n");\
      abort();						\
    }							\
    __tt_comm = current_lvt;				\
    __sync_lock_release(&__tt_lock); }while(0)

#endif



__thread simtime_t current_lvt;

__thread unsigned int current_lp = 0;

__thread unsigned int tid;

/* Total number of cores required for simulation */
unsigned int n_cores;
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot;


/****************************** TODO: REMOVE THIS **********************************************************************************/
static int stop = 0;
extern int StopSimulation(void);
/***********************************************************************************************************************************/


static void process_init_event(void)
{
  ScheduleNewEvent(0, get_timestamp(), 0, 0, 0);
  send_local_outgoing_msgs();  
}

void init(unsigned int _thread_num)
{
  n_cores = _thread_num;
  
  init_communication();
}

void thread_loop(unsigned int thread_id)
{
  msg_t evt;
  int status;
  unsigned int abort_count_1 = 0, abort_count_2 = 0;
  
#ifdef FINE_GRAIN_DEBUG
   unsigned int non_transactional_ex = 0, transactional_ex = 0;
#endif
  
  tid = thread_id;
  
  if(tid == _MAIN_PROCESS)
    process_init_event();
  
  while(!stop)
  {
    if(get_next_message(&evt) == 0)
    {
      if(tid == _MAIN_PROCESS)
	stop = StopSimulation();

      continue;
    }
    
    current_lp = evt.receiver_id;
    current_lvt  = evt.timestamp;
    
    while(1)
    {
      if(check_safety(current_lvt))
      {
	ProcessEvent(current_lp, current_lvt, evt.type, evt.data, evt.data_size, NULL);
#ifdef FINE_GRAIN_DEBUG
	non_transactional_ex++;
#endif
      }
      else
      {
	if( (status = _xbegin()) == _XBEGIN_STARTED)
	{
	  ProcessEvent(current_lp, current_lvt, evt.type, evt.data, evt.data_size, NULL);
	  
	  if(check_safety(current_lvt))
	  {
	    _xend();
#ifdef FINE_GRAIN_DEBUG
	transactional_ex++;
#endif
	  }
	  else
	    _xabort(_ROLLBACK_CODE);
	}
	else
	{
	  status = _XABORT_CODE(status);
	  if(status == _ROLLBACK_CODE)
	    abort_count_1++;
	  else
	    abort_count_2++;
	  continue;
	}
      }
      
      break;
    }

#ifdef FINE_GRAIN_DEBUG
    //CONTROLLO CONSISTENZA
    CHECKER;
#endif
    
    register_local_safe_time();
    send_local_outgoing_msgs();
        
    //printf("Timestamp %f executed\n", evt.timestamp);
  }
  
  printf("Thread %d aborted %u times for cross check condition and %u for memory conflicts\n", 
	 tid, abort_count_1, abort_count_2);
  
#ifdef FINE_GRAIN_DEBUG
  
    printf("Thread %d executed in non-transactional block: %d\n"
    "Thread executed in transactional block: %d\n", 
    tid, non_transactional_ex, transactional_ex);
#endif
  
}















