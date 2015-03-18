#ifndef __EVENT_TYPE_H
#define __EVENT_TYPE_H

#include "time_util.h"


typedef struct __event_t
{  
  unsigned int sender_id;
  unsigned int receiver_id;

  timestamp_t timestamp;
  timestamp_t sender_timestamp; //Potrebbe non servire
  
  /* Tempo da cui dipendo. Se ha committato la transazione che ha processato l'evento da cui questo evento dipende, allora
   * la transazione che sta processando questo evento può a sua volta committare: (cross_check_timestamp == minimum_timestamp_committed)
   */
  timestamp_t cross_check_timestamp;

  void *data;
  unsigned int data_size;
  
  int type;
  
} event_t;



enum EVENT_SYS_TYPE
{
  INIT = 0
};



#endif
