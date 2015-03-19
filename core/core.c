#include <stdio.h>
#include <stdlib.h>
// #include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>

#include "calqueue.h"
#include "core.h"


//id del processo principale
#define _MAIN_PROCESS		0
//Le abort verificate per "cross check condition" avranno questo codice
#define _ROLLBACK_CODE		127


static event_commit_synchronization comm_data;


//Gli LP non sono ancora stati mappati, quindi current_lp_id lo lascio nullo su tutti i thread per adesso
__thread unsigned int current_lp_id = 0;

//Local virtual time dell'evento processato dal thread corrente
__thread timestamp_t current_lvt;


#ifdef _TEST_2 /***********************************************************************/

// timestamp dell'evento dipendente
__thread timestamp_t local_cross_check_timestamp;

// 1 se l'evento processato nel relativo thread è eseguito mediante una transazione
__thread int transactional;

#endif


//Flag per l'interruzione del loop - flag provvisorio - TODO: Se gli LPs non lanciano più eventi interrompi la simulazione
static int stop = 0;


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size);

void init(void);

//Processa l'evento/iniziale - Unico evento processato in modo non transazionale
static void process_init_event(void);

// Gli LP non sono ancora mappati tra i vari threads (l'esperimento prevede un solo LP, quindi 'my_id' per adesso sarà sempre 0)
extern void ProcessEvent(int my_id, timestamp_t now, int event_type, void *data, unsigned int data_size, void *state);

//Torna 1 in caso si verifica la condizione di fine simulazione - Metodo provvisorio
extern int StopSimulation(void);


void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size)
{
  event_t new_event;
  
#ifdef _TEST_2 /***********************************************************************/
  
  if(transactional && (comm_data.minimum_timestamp_committed == local_cross_check_timestamp))
  {
    _xend();
    transactional = 0;
  }
  
#endif
  
  bzero(&new_event, sizeof(event_t));
  
  new_event.sender_id = current_lp_id;
  new_event.receiver_id = receiver;
  new_event.timestamp = timestamp;
  new_event.sender_timestamp = current_lvt;
  new_event.cross_check_timestamp = comm_data.last_timestamp_activated;
  new_event.data = data;
  new_event.data_size = data_size;
  new_event.type = event_type;
  
  //Il prossimo evento generato dipenderà da questo evento appena generato
  comm_data.last_timestamp_activated = timestamp;
  
  // Solo se la transazione andrà in commit l'evento sarà effettivamente inserito nella coda
  
  //TODO IMPORTANTE: new_event va inserito (fisicamente) in una struttura dati dinamica --> creare apposita struttura dati con allocatore
  //WARNING: timestamp_t è uint_64 - calqueue_put prende un tipo double
  calqueue_put((double)new_event.timestamp, &new_event);
}

void init(void)
{
  calqueue_init();
  
  comm_data.last_timestamp_activated = 0;
  comm_data.minimum_timestamp_committed = 0;
}

static void process_init_event(void)
{  
  timestamp_t start_time = get_timestamp(); //WARNING: Attualmente il tempo iniziale non può essere nullo
  
  current_lp_id = 0;
  current_lvt  = start_time;
  //Init è il primo evento ad essere attivato
  comm_data.last_timestamp_activated = current_lvt;
  
#ifdef _TEST_2 /***********************************************************************/
  
  local_cross_check_timestamp = 0;
  transactional = 0;
  
#endif
  
  //Andrà lanciata su tutti gli LP
  ProcessEvent(0, start_time, INIT, NULL, 0, NULL);
  //Init event scrive per primo "l'ultimo" timestamp committato
  comm_data.minimum_timestamp_committed = current_lvt;
}


#ifdef _TEST_1 /***********************************************************************/

void thread_loop(unsigned int thread_id)
{
  event_t *evt;
  unsigned int cross_check_abort_counter = 0, conflict_abort_counter = 0;
  int status;
  
  //Variabile wait momentanea - in attesa di un ottimizzazione più raffinata
  int wait;

  //Il main thread processa l'evento iniziale in modo non-transazionale
  if(thread_id == _MAIN_PROCESS)
    process_init_event();

  //Inizio della simulazione
  while(!stop)
  {
    /* Preleva l'evento a tempo minimo presente sulla coda. - La rimozione deve essere thread-safe
     * WARNING: L'accesso alle strutture dati della coda provoca un immediata abort a tutte le transazioni che tentano di inserire altri eventi nella coda;
     * Quindi, quando la coda è vuota il continuo accesso al metodo queue_min costringe tutte le transazioni in esecuzione ad abortire, con spiacevoli risultati sulle prestazioni
     */
    if( (evt = calqueue_get()) == NULL)
    {
      // Per risolvere in modo molto provvisorio inserisco delle attese - TODO: Intervenire direttamente nelle strutture dati della coda di priorità (ad esempio isolando le rimozioni dagli inserimenti)
      wait = 10000;
      while(wait--)
	_mm_pause();
      
      //StopSimulation provvisorio: torna 1 se la simulazione è finita
      if(thread_id == _MAIN_PROCESS)
	stop = StopSimulation();
      
      continue;
    }

    current_lp_id = evt->receiver_id;
    current_lvt  = evt->timestamp;
    
    //Start transaction
    while(1)
    {
      if( (status = _xbegin()) == _XBEGIN_STARTED)
      {	
	ProcessEvent(current_lp_id, current_lvt, evt->type, evt->data, evt->data_size, NULL); //void *state => Attualmente nullo
	
	/* Se l'evento precedente a questo non ha ancora effettuato il commit devo abortire
	 * In tal modo tutti gli eventi che ho lanciato non saranno mai eseguiti
	 * In'oltre questa condizione fa si che le transazioni "committano in modo seriale"
	 */
	if(comm_data.minimum_timestamp_committed != evt->cross_check_timestamp)
	  _xabort(_ROLLBACK_CODE);
	
	_xend();
	
	//COMMITTED
	
	/* if(current_lvt < comm_data.minimum_timestamp_committed) => l'evento appena committato non può essere più piccolo di un evento committato in un tempo precedente a questo
	 * Questa condizione non sarà mai vera
	 */
	
	break;
      }
      else //Abort
      {
	status = _XABORT_CODE(status);
	if(status == _ROLLBACK_CODE)
	  cross_check_abort_counter++;
	else
	  conflict_abort_counter++;
	
	//Stesso problema di prima nelle attese - soluzione provvisoria
	wait = 10000;
	while(wait--)
	  _mm_pause();
	
	continue;
      }
    }
    
    /* Serialmente ogni thread che ha committato la transazione scrive qui il timestamp dell'evento processato 
     * questo corrisponde all'ultimo evento eseguito e committato fino a questo istante
     */
    comm_data.minimum_timestamp_committed = current_lvt;

  }
  
  printf("Thread %d aborted %u times for cross check condition and %u for memory conflicts\n", thread_id, cross_check_abort_counter, conflict_abort_counter);
}

#endif


#ifdef _TEST_2 /***********************************************************************/

void thread_loop(unsigned int thread_id)
{
  event_t *evt;
  unsigned int cross_check_abort_counter = 0, conflict_abort_counter = 0, committed_transaction_counter = 0, serial_execution_counter = 0;
  int status;
  
  if(thread_id == _MAIN_PROCESS)
    process_init_event();
  
  while(!stop)
  {
    if( (evt = calqueue_get()) == NULL)
    {
      if(thread_id == _MAIN_PROCESS)
	stop = StopSimulation();
      
      continue;
    }
    
    //printf("%u Estratto evento %lu\n", thread_id, evt.timestamp);
    
    current_lp_id = evt->receiver_id;
    current_lvt  = evt->timestamp;
    local_cross_check_timestamp = evt->cross_check_timestamp;
    
    while(1)
    {
      transactional = 0;
      
      if(comm_data.minimum_timestamp_committed == local_cross_check_timestamp)
      {
	ProcessEvent(current_lp_id, current_lvt, evt->type, evt->data, evt->data_size, NULL);
	serial_execution_counter++;
      }
      else
      {
	transactional = 1;
	
	if( (status = _xbegin()) == _XBEGIN_STARTED)
	{
	  ProcessEvent(current_lp_id, current_lvt, evt->type, evt->data, evt->data_size, NULL);
	  
	  if(transactional)
	  {
	    if(comm_data.minimum_timestamp_committed != local_cross_check_timestamp)
	      _xabort(_ROLLBACK_CODE);

	    _xend();
	    
	    committed_transaction_counter++;
	  }
	}
	else
	{
	  status = _XABORT_CODE(status);
	  	  
	  if(status == _ROLLBACK_CODE)
	    cross_check_abort_counter++;
	  else
	    conflict_abort_counter++;
	  
	  continue;
	}
      }
      
      break;
    }
    
    comm_data.minimum_timestamp_committed = current_lvt;
    
    //printf("%u: Completato evento %lu\n", thread_id, current_lvt);
  }
  
  printf("Thread %u: Aborts for memory conflict  %u\n", thread_id, conflict_abort_counter);
  printf("Thread %u: Aborts for check condition  %u\n", thread_id, cross_check_abort_counter);
  printf("Thread %u: Transactions committed      %u\n", thread_id, committed_transaction_counter);
  printf("Thread %u: Serials execution executed  %u\n", thread_id, serial_execution_counter);
}

#endif
