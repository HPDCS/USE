#pragma once
#ifndef __COMMUNICATION_H
#define __COMMUNICATION_H


#include "core.h"


#define MAX_OUTGOING_MSG	50


typedef struct __outgoing_t
{
  msg_t outgoing_msgs[MAX_OUTGOING_MSG];
  
  simtime_t min_in_transit;
  unsigned int size;
  
} outgoing_t;

// Initialize communication module
extern void init_communication(void);

// Commit the safe time for "this" thread
extern void register_local_safe_time(void);

// Deliver outgoing messages from thread
extern void send_local_outgoing_msgs(void);

// Get next message from the calqueue and set the local execution time
extern int get_next_message(msg_t *msg);


#endif
