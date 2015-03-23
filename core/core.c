#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>

#include "core.h"
#include "queue.h"
#include "message_state.h"
#include "event_type.h"
#include "datatypes.h"


//id del processo principale
#define _MAIN_PROCESS		0
//Le abort "volontarie" avranno questo codice
#define _ROLLBACK_CODE		127


// Local thread id
__thread unsigned int tid;

//Gli LP non sono ancora stati mappati, quindi current_lp_id lo lascio nullo su tutti i thread per adesso
__thread unsigned int current_lp_id = 0;

//Local virtual time dell'evento processato dal thread corrente
__thread timestamp_t current_lvt;

unsigned int n_cores;

//Flag per l'interruzione del loop - flag provvisorio - TODO: Se gli LPs non lanciano più eventi interrompi la simulazione
static int stop = 0;


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size);

// Gli LP non sono ancora mappati tra i vari threads (l'esperimento prevede un solo LP, quindi 'my_id' per adesso sarà sempre 0)
extern void ProcessEvent(int my_id, timestamp_t now, int event_type, void *data, unsigned int data_size, void *state);

//Torna 1 in caso si verifica la condizione di fine simulazione - Metodo provvisorio
extern int StopSimulation(void);


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size)
{
  event_t new_event;
  bzero(&new_event, sizeof(event_t));
  
  new_event.sender_id = current_lp_id;
  new_event.receiver_id = receiver;
  new_event.timestamp = timestamp;
  new_event.sender_timestamp = current_lvt;
  new_event.data = data;
  new_event.data_size = data_size;
  new_event.type = event_type;
  
  queue_insert(&new_event);
}

static void process_init_event(void)
{
  ScheduleNewEvent(0, get_timestamp(), INIT, 0, 0);
  queue_deliver_msgs();
  
}

void init(unsigned int _thread_num)
{
  n_cores = _thread_num;
  
  queue_init();
  message_state_init();
}

/* Loop eseguito dai singoli thread - Ogni thread in una transazione processa l'evento a tempo minimo estratto dalla coda,
 * e quest'ultimo protrà effettuare il commit solo se il primo evento lanciato precedentemente a questo ha effettuato il commit, ovvero sarà l'ultimo evento ad aver committato.
 */
void thread_loop(unsigned int thread_id)
{
  event_t evt;
  int status;
  
  unsigned int abort_count_1 = 0, abort_count_2 = 0;
  
#ifdef FINE_GRAIN_DEBUG
  static unsigned int non_transaction_ex = 0;
  static unsigned int transaction_ex = 0;
#endif
  
  tid = thread_id;
  queue_register_thread();
  
  if(tid == _MAIN_PROCESS)
    process_init_event();
  
  while(!stop)
  {
    if(queue_min(&evt) == 0)
    {
      if(tid == _MAIN_PROCESS)
	stop = StopSimulation();

      continue;
    }
    
    current_lp_id = evt.receiver_id;
    current_lvt  = evt.timestamp;
    
    //setta a current_lvt e azzera l'outgoing message
    execution_time(current_lvt);
    
    while(!stop)
    {
      /***** WARNING: L'Attuale modello usato nel test usa strutture dati condivise => Prima causa di eccesive abort *****/
      int wait = 1000000;
      while(wait--);
      
      if(check_safety(current_lvt))
      {
#ifdef FINE_GRAIN_DEBUG
	__sync_fetch_and_add(&non_transaction_ex, 1);
#endif
	
	ProcessEvent(current_lp_id, current_lvt, evt.type, evt.data, evt.data_size, NULL);
	
	min_output_time(queue_pre_min()->timestamp);//TODO (metti il timestamp minore uscente da Process event
	commit_time();
	queue_deliver_msgs();
      }
      else
      {
	if( (status = _xbegin()) == _XBEGIN_STARTED)
	{
	  ProcessEvent(current_lp_id, current_lvt, evt.type, evt.data, evt.data_size, NULL);
	  
	  if(check_safety(evt.timestamp))
	  {	
	    _xend();
	    
	    min_output_time(queue_pre_min()->timestamp);//TODO (metti il timestamp minore uscente da Process event
	    commit_time();
	    queue_deliver_msgs();
	    
#ifdef FINE_GRAIN_DEBUG
	    __sync_fetch_and_add(&transaction_ex, 1);
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
    
    //printf("Timestamp %lu executed\n", evt.timestamp);
  }
  
  printf("Thread %d aborted %u times for cross check condition and %u for memory conflicts\n", 
	 tid, abort_count_1, abort_count_2);

#ifdef FINE_GRAIN_DEBUG
  if(tid == _MAIN_PROCESS)
    printf("Thread executed in non-transactional block: %d\n"
    "Thread executed in transactional block: %d\n", 
    non_transaction_ex, transaction_ex);
#endif
  
}


