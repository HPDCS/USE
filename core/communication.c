#include <calqueue.h>
#include "message_state.h"

#include "communication.h"



outgoing_t *outgoing_queue;

int comm_lock = 0;


// Insert a message into the local queue
static void insert_local_outgoing_msg(msg_t *msg);
// Insert a message into the shared pool
static void insert_message(msg_t *msg);
// Remove the message pointed by the given pointer
static void remove_message(msg_t *pointer);


void init_communication(void)
{
  int i;
  
  calqueue_init();
  message_state_init();
  
  outgoing_queue = malloc(sizeof(outgoing_t) * n_cores);
  for(i = 0; i < n_cores; i++)
  {
    outgoing_queue[i].min_in_transit = INFTY;
    outgoing_queue[i].size = 0;
  }
}

void ScheduleNewEvent(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
  msg_t new_event;
  bzero(&new_event, sizeof(msg_t));
  
  new_event.sender_id = current_lp;
  new_event.receiver_id = receiver;
  new_event.timestamp = timestamp;
  new_event.sender_timestamp = current_lvt;
  new_event.data = event_content;
  new_event.data_size = event_size;
  new_event.type = event_type;
  new_event.who_generated = tid;
  
  insert_local_outgoing_msg(&new_event);
}

void insert_local_outgoing_msg(msg_t *msg)
{
  if(outgoing_queue[tid].size == MAX_OUTGOING_MSG)
  {
    printf("queue error _________________________ \n");
    abort();
    return; //TODO error
  }
    
  if(msg->timestamp < outgoing_queue[tid].min_in_transit)
    outgoing_queue[tid].min_in_transit = msg->timestamp;
    
  memcpy(&(outgoing_queue[tid].outgoing_msgs[outgoing_queue[tid].size]), msg, sizeof(msg_t));
  outgoing_queue[tid].size++;
}

void register_local_safe_time(void)
{
  if(outgoing_queue[tid].size)
    min_output_time(outgoing_queue[tid].min_in_transit);
//  commit_time();
}

// Deliver outgoing messages from thread
void send_local_outgoing_msgs(void)
{
  msg_t *tmp;
  int i;
  
  while(__sync_lock_test_and_set(&comm_lock, 1))
    while(comm_lock);
  
  for(i = 0; i < outgoing_queue[tid].size; i++)
  {
    tmp = &(outgoing_queue[tid].outgoing_msgs[i]);
    insert_message(tmp);
  }
  
  __sync_lock_release(&comm_lock);
  
  outgoing_queue[tid].min_in_transit = INFTY;
  outgoing_queue[tid].size = 0;
}

int get_next_message(msg_t *msg)
{  
  msg_t *tmp;
  
  // spin-lock ottimizzabile
  
  while(__sync_lock_test_and_set(&comm_lock, 1))
    while(comm_lock);  
  
  tmp = calqueue_get();
  if(tmp == NULL)
  {
    __sync_lock_release(&comm_lock);
    return 0;
  }
  
  execution_time(tmp->timestamp, tmp->who_generated);  
  
  memcpy(msg, tmp, sizeof(msg_t));
  remove_message(tmp);
  
  __sync_lock_release(&comm_lock);
  
  return 1;
}

void insert_message(msg_t *msg)
{
  msg_t *msg_block;
  
  msg_block = malloc(sizeof(msg_t));	//TODO: ALLOCATOR (importante)
  
  memcpy(msg_block, msg, sizeof(msg_t));
  calqueue_put(msg->timestamp, msg_block);
}

void remove_message(msg_t *pointer)
{
  free(pointer);
}



