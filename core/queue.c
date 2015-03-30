
/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Estrazione Thread-safe (non lock-free)
 * Inserimenti avvengono in transazioni
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h" 
#include "message_state.h"
#include "calqueue.h"
#include "core.h"



typedef struct __event_pool_node
{
  msg_t message;
  calqueue_node *calqueue_node_reference;
  
} event_pool_node;

typedef struct __queue_t 
{
  event_pool_node *head;
  unsigned int size;
  
} queue_t;

/*
typedef struct __temp_thread_pool
{
  msg_t *_tmp_mem __attribute__ ((aligned (64)));
  void *_tmp_msg_data;
  void *curr_msg_data;
  simtime_t min_time;
  unsigned int non_commit_size;
  
} temp_thread_pool;
*/

__thread msg_t current_msg __attribute__ ((aligned (64)));


queue_t _queue;

typedef struct __temp_thread_pool {
	unsigned int _thr_pool_count;
	msg_t messages[THR_POOL_SIZE]  __attribute__ ((aligned (64)));
	simtime_t min_time;
} __temp_thread_pool;

__thread __temp_thread_pool _thr_pool  __attribute__ ((aligned (64)));

int queue_lock = 0;

void queue_init(void)
{  
  calqueue_init();
  
 /* _queue.head = malloc(sizeof(msg_t) * MAX_SIZE);
  
  if(_queue.head == 0)
  {
    fprintf(stderr, "queue_init: Out of memory\n");
    abort();
  }
  
  _queue.size = 0;*/
}
/*
void queue_register_thread(void)
{
  if(_thr_pool != 0)
    return;
  
  _thr_pool = malloc(sizeof(temp_thread_pool)); //TODO: Togliere malloc
  _thr_pool->_tmp_mem = malloc(sizeof(msg_t) * THR_POOL_SIZE);
  _thr_pool->_tmp_msg_data = malloc(MAX_DATA_SIZE);
  _thr_pool->curr_msg_data = _thr_pool->_tmp_msg_data;
  _thr_pool->min_time = INFTY;
  _thr_pool->non_commit_size = 0;
}
*/
void queue_insert(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
  msg_t *msg_ptr;

  if(_thr_pool._thr_pool_count == THR_POOL_SIZE)
  {
    printf("queue overflow: inserting event %d over %d\n", _thr_pool._thr_pool_count, THR_POOL_SIZE);
    printf("-----------------------------------------------------------------\n");
    abort();
  }

  if(event_size > MAX_DATA_SIZE) {
	printf("Error: requested a message of size %d, max allowed is %d\n", event_size, MAX_DATA_SIZE);
	abort();
  }
  
  if(timestamp < _thr_pool.min_time)
    _thr_pool.min_time = timestamp;

  msg_ptr = &_thr_pool.messages[_thr_pool._thr_pool_count++];
  
  msg_ptr->sender_id = current_lp;
  msg_ptr->receiver_id = receiver;
  msg_ptr->timestamp = timestamp;
  msg_ptr->data_size = event_size;
  msg_ptr->type = event_type;

  memcpy(msg_ptr->data, event_content, event_size);
}

double queue_pre_min(void)
{
  return _thr_pool.min_time;
}

double queue_deliver_msgs(void)
{
//  while(__sync_lock_test_and_set(&queue_lock, 1))
//    while(queue_lock);


  
  msg_t *new_hole;
  int i;
  double mintime;
  
  for(i = 0; i < _thr_pool._thr_pool_count; i++)
  {
    new_hole = malloc(sizeof(msg_t));
    memcpy(new_hole, &_thr_pool.messages[i], sizeof(msg_t));
    calqueue_put(new_hole->timestamp, new_hole);
  }

  mintime = _thr_pool.min_time;
  
  _thr_pool._thr_pool_count = 0;
  _thr_pool.min_time = INFTY;
  
  return mintime;
  /*event_pool_node *new_hole;
  unsigned int i;
    
  if(_queue.size == MAX_SIZE)
  {
    __sync_lock_release(&queue_lock);
    
    printf("queue overflow\n");
    printf("-----------------------------------------------------------------\n");
    abort();
  }
  
  for(i = 0; i < _thr_pool->non_commit_size; i++)
  {
    new_hole = (_queue.head + _queue.size);
    memcpy(&(new_hole->message), _thr_pool->_tmp_mem + i, sizeof(msg_t));
    new_hole->calqueue_node_reference = calqueue_put(new_hole->message.timestamp, new_hole);
    _queue.size++;
  }
  
  _thr_pool->non_commit_size = 0;*/
  
//  __sync_lock_release(&queue_lock);
}

int queue_min(void)
{
  //event_pool_node *node_ret;

  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);
  
  msg_t *node_ret;
  node_ret = calqueue_get();
  if(node_ret == NULL)
  {
    __sync_lock_release(&queue_lock);
    return 0;
  }
  
  memcpy(&current_msg, node_ret, sizeof(msg_t));
  free(node_ret);
  
  execution_time(current_msg.timestamp);
  
  __sync_lock_release(&queue_lock);
  
  return 1;
  
    /*
  if(_queue.size == 0)
  {
    __sync_lock_release(&queue_lock);
    return 0;
  }

  node_ret = calqueue_get();
  memcpy(ret_evt, &(node_ret->message), sizeof(msg_t));
  
  if((_queue.head + (_queue.size - 1)) != node_ret)
  {
    memcpy(node_ret, (_queue.head + (_queue.size - 1)), sizeof(event_pool_node));
    node_ret->calqueue_node_reference->payload = node_ret;
  }
  
  _queue.size--;
    
  //setta a current_lvt e azzera l'outgoing message
  execution_time(ret_evt->timestamp, ret_evt->who_generated);
  
  __sync_lock_release(&queue_lock);
  
  return 1;*/
}

/*
int queue_pending_message_size(void)
{
  return _thr_pool->non_commit_size;
}
*/

void queue_destroy(void)
{
  free(_queue.head);
}
