#include <stdio.h>
#include <stdlib.h>
// #include <sched.h>
#include <pthread.h>
#include <string.h>
#include <immintrin.h>

#include "core.h"


//id del processo principale
#define _MAIN_PROCESS		0
//Le abort verificate per "cross check condition" avranno questo codice
#define _ROLLBACK_CODE		127


static event_commit_synchronization comm_data;

static scheduler_data sched;

//Gli LP non sono ancora stati mappati, quindi current_lp_id lo lascio nullo su tutti i thread per adesso
__thread unsigned int current_lp_id = 0;

//Local virtual time dell'evento processato dal thread corrente
__thread timestamp_t current_lvt;

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
  // Schedulo un nuovo evento - questa funzione viene chiamata all'interno di una transazione
  
  event_t new_event;
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
  queue_insert(sched.event_queue, &new_event);
}

void init(void)
{
  sched.event_queue = queue_init();
  
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
  
  //Andrà lanciata su tutti gli LP
  ProcessEvent(0, start_time, INIT, NULL, 0, NULL);
  //Init event scrive per primo "l'ultimo" timestamp committato
  comm_data.minimum_timestamp_committed = current_lvt;
}

/* Loop eseguito dai singoli thread - Ogni thread in una transazione processa l'evento a tempo minimo estratto dalla coda,
 * e quest'ultimo protrà effettuare il commit solo se il primo evento lanciato precedentemente a questo ha effettuato il commit, ovvero sarà l'ultimo evento ad aver committato.
 */
void thread_loop(unsigned int thread_id)
{
  event_t evt;
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
    if(queue_min(sched.event_queue, &evt) == 0)
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

    current_lp_id = evt.receiver_id;
    current_lvt  = evt.timestamp;
    
    //Start transaction
    while(1)
    {
      if( (status = _xbegin()) == _XBEGIN_STARTED)
      {	
	ProcessEvent(current_lp_id, current_lvt, evt.type, evt.data, evt.data_size, NULL); //void *state => Attualmente nullo
	
	/* Se l'evento precedente a questo non ha ancora effettuato il commit devo abortire
	 * In tal modo tutti gli eventi che ho lanciato non saranno mai eseguiti
	 * In'oltre questa condizione fa si che le transazioni "committano in modo seriale"
	 */
	if(comm_data.minimum_timestamp_committed != evt.cross_check_timestamp)
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

