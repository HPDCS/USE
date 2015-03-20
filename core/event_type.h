#ifndef __EVENT_TYPE_H
#define __EVENT_TYPE_H

#include "time_util.h"


typedef struct __event_t
{  
  unsigned int sender_id;
  unsigned int receiver_id;

  timestamp_t timestamp;
  timestamp_t sender_timestamp; //Potrebbe non servire
  
  void *data;
  unsigned int data_size;
  
  int type;
  
} event_t;



enum EVENT_SYS_TYPE
{
  INIT = 0
};



#endif
