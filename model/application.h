#ifndef __APPLICATION_H
#define __APPLICATION_H


#include "event_type.h"


extern void ScheduleNewEvent(unsigned int receiver, timestamp_t timestamp, int event_type, void *data, unsigned int data_size);


void ProcessEvent(int my_id, timestamp_t now, int event_type, void *data, unsigned int data_size, void *state);

//Torna 1 in caso si verifica la condizione di fine simulazione
int StopSimulation(void); 


#endif
