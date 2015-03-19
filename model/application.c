#include <stdio.h>
#include <stdlib.h>


/* SIMULATION TEST */

#include "application.h"


#define STOP_COUNTER	1024
#define PCK_EVENT 	1
#define ERROR_EVENT	2


static int error_flag = 0;
static int counter;


void ProcessEvent(int my_id, simtime_t now, int event_type, void *data, unsigned int data_size, void *state)
{
  simtime_t new_time;
  
  switch(event_type)
  {
    case INIT:
    {
      unsigned long int *p = malloc(sizeof(long));
      
      counter = 0;
            
      //Sending new event
      new_time = get_timestamp();
      *p = now;
      ScheduleNewEvent(0, new_time, PCK_EVENT, p, sizeof(long));
      
      break;
    }
    
    case PCK_EVENT:
    {
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
      
      break;
    }
    
    case ERROR_EVENT:
    {
      error_flag = 1;
      break;
    }
  }
}

bool OnGVT(void)
{
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
  
  return 0;
}
