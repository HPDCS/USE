#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ROOT-Sim.h>
#include <dymelor.h>
#include <numerical.h>

#include "core.h"
#include "queue.h"
#include "message_state.h"
#include "simtypes.h"


//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127


#define MAX_PATHLEN	512


__thread simtime_t current_lvt = 0;

__thread unsigned int current_lp = 0;

__thread unsigned int tid = 0;

unsigned long long tid0_evt_count = 0;

/* Total number of cores required for simulation */
unsigned int n_cores;
/* Total number of logical processes running in the simulation */
unsigned int n_prc_tot;

bool stop = false;
bool sim_error = false;


void **states;

bool *can_stop;


void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);\
	fflush(stderr);

	if(fatal) {
		// Notify all KLT to shut down the simulation
		sim_error = true;
	}
}


/**
* This is an helper-function to allow the statistics subsystem create a new directory
*
* @author Alessandro Pellegrini
*
* @param path The path of the new directory to create
*/
void _mkdir(const char *path) {

	char opath[MAX_PATHLEN];
	char *p;
	size_t len;

	strncpy(opath, path, sizeof(opath));
	len = strlen(opath);
	if(opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// opath plus 1 is a hack to allow also absolute path
	for(p = opath + 1; *p; p++) {
		if(*p == '/') {
			*p = '\0';
			if(access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if(access(opath, F_OK)) {
		if (mkdir(opath, S_IRWXU)) {
			if (errno != EEXIST) {
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
			}
		}
	}
}



void SetState(void *ptr) {
	states[current_lp] = ptr;
}

static void process_init_event(void) {
  unsigned int i;

  for(i = 0; i < n_prc_tot; i++) {
    current_lp = i;
    current_lvt = 0;
    ProcessEvent(current_lp, current_lvt, INIT, NULL, 0, states[current_lp]);
  }
  
  queue_deliver_msgs(); 
}

void init(unsigned int _thread_num, unsigned int lps_num)
{
  printf("Starting an execution with %u threads, %u LPs\n", _thread_num, lps_num);
  n_cores = _thread_num;
  n_prc_tot = lps_num;
  states = malloc(sizeof(void *) * n_prc_tot);
  can_stop = malloc(sizeof(bool) * n_prc_tot);
 
  dymelor_init();
  queue_init();
  message_state_init();
  numerical_init();
  
  queue_register_thread();
  process_init_event();
}


bool check_termination(void) {
	int i;
	bool ret = true;
	for(i = 0; i < n_prc_tot; i++) {
		ret &= can_stop[i];
	}
	return ret;
}

void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
  /*msg_t new_event;
  bzero(&new_event, sizeof(msg_t));
 
  while(__sync_lock_test_and_set(&a, 1))
    while(a);
  void *ptr;   
  ptr = malloc(event_size);
  memcpy(ptr, event_content, event_size);
  
  __sync_lock_release(&a);

  new_event.sender_id = current_lp;
  new_event.receiver_id = receiver;
  new_event.timestamp = timestamp;
  new_event.sender_timestamp = current_lvt;
  new_event.data = ptr;
  new_event.data_size = event_size;
  new_event.type = event_type;
  new_event.who_generated = tid;
  */
  
  queue_insert(receiver, timestamp, event_type, event_content, event_size);
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
  
  if(tid != _MAIN_PROCESS)
    queue_register_thread();
  
  while(!stop && !sim_error)
  {
    if(queue_min(&evt) == 0)
    {
      continue;
    }
    
    if(tid == _MAIN_PROCESS)
	stop = check_termination();

    current_lp = evt.receiver_id;
    current_lvt  = evt.timestamp;

    while(1)
    {
      if(check_safety(current_lvt))
      {
	ProcessEvent(current_lp, current_lvt, evt.type, evt.data, evt.data_size, states[current_lp]);
#ifdef FINE_GRAIN_DEBUG
	__sync_fetch_and_add(&non_transactional_ex, 1);
#endif
      }
      else
      {
	if( (status = _xbegin()) == _XBEGIN_STARTED)
	{
	  ProcessEvent(current_lp, current_lvt, evt.type, evt.data, evt.data_size, states[current_lp]);
	  
	  if(check_safety(current_lvt))
	  {
	    _xend();
#ifdef FINE_GRAIN_DEBUG
	__sync_fetch_and_add(&transactional_ex, 1);
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
   

    flush();
 
/*    if(queue_pending_message_size())
      min_output_time(queue_pre_min());
    commit_time();    
    queue_deliver_msgs();
  */  
    //Libero la memoria allocata per i dati dell'evento
    free(evt.data);

    can_stop[current_lp] = OnGVT(current_lp, states[current_lp]);

    if(tid == _MAIN_PROCESS) {
	tid0_evt_count++;
	if(tid0_evt_count % 10000 == 0)
		printf("TIME: %f\n", current_lvt);
    }
        
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















