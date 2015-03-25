#include <stdio.h>
#include <stdlib.h>


/* SIMULATION TEST */

#include "application.h"


#define STOP_COUNTER	256
#define PCK_EVENT 	1
#ifdef TEST_2
#define ERROR_EVENT	2
#endif

#ifdef TEST_1
__thread int per_thread_counter = 0;
#endif


#ifdef TEST_2
static int error_flag = 0;
static int counter;
#endif


void ProcessEvent(int my_id, timestamp_t now, int event_type, void *data, unsigned int data_size, void *state)
{
  switch(event_type)
  {
    case INIT:
    {
#ifdef TEST_1
      ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, NULL, 0);
#endif
      
      
#ifdef TEST_2
      unsigned long int *p = malloc(sizeof(long));
      
      counter = 0;
            
      //Sending new event
      *p = now;
      ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, p, sizeof(long));
#endif
      
      break;
    }
    
    case PCK_EVENT:
    {
#ifdef TEST_1
      if(per_thread_counter < STOP_COUNTER)
      {
	per_thread_counter++;
	
	ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, NULL, 0);
	ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, NULL, 0);
      }
#endif
      
#ifdef TEST_2

      if(counter < STOP_COUNTER)
      {	
	// p contiene il timestamp dell'ultimo evento eseguito la cui transazione è andata in commit
	unsigned long int *p = (unsigned long int*)data;
	
	//"Transazionalmente" imposto il valore puntato da p uguale al mio timestamp (Il vecchio valore di *p deve essere sempre minore del mio timestamp)
	if(*p < now)
	  *p = now;
	else //NON SI DEVE VERIFICARE
	  ScheduleNewEvent(0, get_timestamp(), ERROR_EVENT, 0, 0);

	//Incremento un contatore
	counter++;

	//Schedulo due nuovi eventi
	ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, p, sizeof(long));
	ScheduleNewEvent(0, get_timestamp(), PCK_EVENT, p, sizeof(long));
	
	//QUESTA CONDIZIONE NON SI DOVRÀ MAI VERIFICARE - Un evento con timestamp maggiore del mio (now) non può andare in commit prima di me
	if(*p > now)
	{
	  ScheduleNewEvent(0, get_timestamp(), ERROR_EVENT, 0, 0);
	  break;
	}
	
      }
#endif
      
      break;
    }
    
#ifdef TEST_2
    case ERROR_EVENT:
    {
      error_flag = 1;
      break;
    }
#endif
  }
}

int StopSimulation(void)
{
#ifdef TEST_1
  if(per_thread_counter >= STOP_COUNTER)
    return 1;
#endif
  
#ifdef TEST_2
  if(error_flag)
  {
    printf("Simulation error ------------ \n");
    return 1;
  }
  
  if(counter >= STOP_COUNTER)
  {
    printf("count = %d\n", counter);
    return 1;
  }
#endif
  
  return 0;
}
