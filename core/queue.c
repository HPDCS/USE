
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


//#define MAX_SIZE		65536
#define MAX_DATA_SIZE		65536
#define THR_POOL_SIZE		65536

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

typedef struct __temp_thread_pool
{
  msg_t *_tmp_mem;
  void *_tmp_msg_data;
  void *curr_msg_data;
  simtime_t min_time;
  unsigned int non_commit_size;
  
} temp_thread_pool;


queue_t _queue;
__thread temp_thread_pool *_thr_pool = 0;

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

void queue_insert(unsigned int receiver, simtime_t timestamp, unsigned int event_type, void *event_content, unsigned int event_size)
{
  msg_t *msg_ptr;
  void *data_ptr;
  
  if(_thr_pool->non_commit_size == THR_POOL_SIZE)
  {
    printf("queue overflow\n");
    printf("-----------------------------------------------------------------\n");
    abort();
  }
  
  if(timestamp < _thr_pool->min_time)
    _thr_pool->min_time = timestamp;
  
  msg_ptr = _thr_pool->_tmp_mem + _thr_pool->non_commit_size;
  bzero(msg_ptr, sizeof(msg_t));
  
  data_ptr = _thr_pool->curr_msg_data;
  bzero(data_ptr, event_size);
  memcpy(data_ptr, event_content, event_size);
  _thr_pool->curr_msg_data += event_size;
  
  msg_ptr->sender_id = current_lp;
  msg_ptr->receiver_id = receiver;
  msg_ptr->timestamp = timestamp;
  msg_ptr->sender_timestamp = current_lvt;
  msg_ptr->data = data_ptr;
  msg_ptr->data_size = event_size;
  msg_ptr->type = event_type;
  msg_ptr->who_generated = tid;
  
  _thr_pool->non_commit_size++;
      
  /*memcpy((_thr_pool->_tmp_mem + _thr_pool->non_commit_size), msg, sizeof(msg_t));
    
  _thr_pool->non_commit_size++;*/
}

double queue_pre_min(void)
{
  return _thr_pool->min_time;
}

void queue_deliver_msgs(void)
{
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);
  
  msg_t *new_hole;
  void *_data;
  int i;
  
  _thr_pool->curr_msg_data = _thr_pool->_tmp_msg_data;
  
  for(i = 0; i < _thr_pool->non_commit_size; i++)
  {
    new_hole = malloc(sizeof(msg_t));
    memcpy(new_hole, _thr_pool->_tmp_mem + i, sizeof(msg_t));
    calqueue_put(new_hole->timestamp, new_hole);
    
    _data = malloc(new_hole->data_size);
    memcpy(_data, _thr_pool->curr_msg_data, new_hole->data_size);
    new_hole->data = _data;
    _thr_pool->curr_msg_data += new_hole->data_size;
  }
  
  _thr_pool->curr_msg_data = _thr_pool->_tmp_msg_data;
  _thr_pool->non_commit_size = 0;
  _thr_pool->min_time = INFTY;
  
    
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
  
  __sync_lock_release(&queue_lock);
}

int queue_min(msg_t *ret_evt)
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
  
  memcpy(ret_evt, node_ret, sizeof(msg_t));
  free(node_ret);
  
  execution_time(ret_evt->timestamp, ret_evt->who_generated);
  
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

int queue_pending_message_size(void)
{
  return _thr_pool->non_commit_size;
}

void queue_destroy(void)
{
  free(_queue.head);
}
