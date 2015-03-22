#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>

#include "core.h"
#include "calqueue.h"
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

//Flag per l'interruzione del loop - flag provvisorio - TODO: Se gli LPs non lanciano più eventi interrompi la simulazione
static int stop = 0;


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size);

void init(void);

// Gli LP non sono ancora mappati tra i vari threads (l'esperimento prevede un solo LP, quindi 'my_id' per adesso sarà sempre 0)
extern void ProcessEvent(int my_id, timestamp_t now, int event_type, void *data, unsigned int data_size, void *state);

//Torna 1 in caso si verifica la condizione di fine simulazione - Metodo provvisorio
extern int StopSimulation(void);


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size)
{
  /*
  event_t new_event;
  bzero(&new_event, sizeof(event_t));
  
  new_event.sender_id = current_lp_id;
  new_event.receiver_id = receiver;
  new_event.timestamp = timestamp;
  new_event.sender_timestamp = current_lvt;
  new_event.data = data;
  new_event.data_size = data_size;
  new_event.type = event_type;
  */
}

void init(void)
{
  calqueue_init();
  message_state_init();
}

/* Loop eseguito dai singoli thread - Ogni thread in una transazione processa l'evento a tempo minimo estratto dalla coda,
 * e quest'ultimo protrà effettuare il commit solo se il primo evento lanciato precedentemente a questo ha effettuato il commit, ovvero sarà l'ultimo evento ad aver committato.
 */
void thread_loop(unsigned int thread_id)
{
  event_t *evt;
  int status;
  
  tid = thread_id;
  
  while(stop)
  {
    if( (evt = calqueue_get()) == NULL)
    {
      if(tid == _MAIN_PROCESS)
	stop = StopSimulation();
      
      continue;
    }
    
    current_lp_id = evt->receiver_id;
    current_lvt  = evt->timestamp;
    
    //setta a current_lvt e azzera l'outgoing message
    execution_time(current_lvt);
    
    if(check_safety(current_lvt))
    {
      ProcessEvent(current_lp_id, current_lvt, evt->type, evt->data, evt->data_size, NULL);
      
      min_output_time( ... );//TODO (metti il timestamp minore uscente da Process event
      commit_time();
      deliver_msgs();
    }
    else
    {
      if( (status = _xbegin()) == _XBEGIN_STARTED)
      {
	ProcessEvent(current_lp_id, current_lvt, evt->type, evt->data, evt->data_size, NULL);
	
	if(check_safety(evt->timestamp))
	{	
	  _xend();
	  
	  min_output_time();//TODO (metti il timestamp minore uscente da Process event
	  commit_time();
	  deliver_msgs();
	}
	else
	  _xabort(_ROLLBACK_CODE);
      }
      else
	continue;
    }
  }
}











