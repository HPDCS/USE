#ifndef __APPLICATION_H
#define __APPLICATION_H

#include <ROOT-Sim.h>

void ProcessEvent(int my_id, simtime_t now, int event_type, void *data, unsigned int data_size, void *state);

#endif
