#pragma once
#ifndef __SIMTYPES_H
#define __SIMTYPES_H

#include <float.h>


/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX


/// This defines the type with whom timestamps are represented
typedef double simtime_t;



typedef struct __msg_t
{
  unsigned int sender_id;	//Sednder LP
  unsigned int receiver_id;	//Receiver LP
  simtime_t timestamp;
  int type;
  unsigned int data_size;
  unsigned char data[MAX_DATA_SIZE];
  revwin_t *revwin;			//reverse window to rollback
  struct __bucket_node * node;	//address of the belonging node
} msg_t;




#endif
