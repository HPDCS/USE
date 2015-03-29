
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
#include "core.h"


#define MAX_SIZE		30000
#define THR_POOL_SIZE		4096


typedef struct __queue_t 
{
  msg_t *head;
  unsigned int size;
  
} queue_t;

typedef struct __temp_thread_pool
{
  msg_t *_tmp_mem;
  unsigned int non_commit_size;
  
} temp_thread_pool;


queue_t _queue;
__thread temp_thread_pool *_thr_pool = 0;

int queue_lock = 0;

void queue_init(void)
{    
  _queue.head = malloc(sizeof(msg_t) * MAX_SIZE);
  
  if(_queue.head == 0)
  {
    fprintf(stderr, "queue_init: Out of memory\n");
    abort();
  }
  
  _queue.size = 0;
}

void queue_register_thread(void)
{
  if(_thr_pool != 0)
    return;
  
  _thr_pool = malloc(sizeof(temp_thread_pool));
  _thr_pool->_tmp_mem = malloc(sizeof(msg_t) * THR_POOL_SIZE);
  _thr_pool->non_commit_size = 0;
}

int queue_insert(msg_t *msg)
{
  if(_thr_pool->non_commit_size == THR_POOL_SIZE)
  {
    printf("queue overflow\n");
    abort();
  }
  
  memcpy((_thr_pool->_tmp_mem + _thr_pool->non_commit_size), msg, sizeof(msg_t));
  _thr_pool->non_commit_size++;
    
  return 1;
}

msg_t *queue_pre_min(void)
{
  return _thr_pool->_tmp_mem;
}

void queue_deliver_msgs(void)
{
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);
    
  if(_queue.size == MAX_SIZE)
  {
    __sync_lock_release(&queue_lock);
    
    printf("queue overflow\n");
    abort();
  }
  
  memcpy((_queue.head + _queue.size), _thr_pool->_tmp_mem, (_thr_pool->non_commit_size * sizeof(msg_t)));
  
  _queue.size += _thr_pool->non_commit_size;
  _thr_pool->non_commit_size = 0;
  
  __sync_lock_release(&queue_lock);
}

int queue_min(msg_t *ret_evt)
{
  msg_t *aux;
  msg_t *min = _queue.head;
  unsigned int i = 1;

  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);
    
  if(_queue.size == 0)
  {
    __sync_lock_release(&queue_lock);
    return 0;
  }

  while(i < _queue.size)
  {
    aux = (_queue.head + i);
    if(aux->timestamp < min->timestamp)
      min = aux;
    i++;
  }
  
  memcpy(ret_evt, min, sizeof(msg_t));
  
  if(_queue.size > 1)
    memcpy(min, (_queue.head + (_queue.size - 1)), sizeof(msg_t));
  
  _queue.size--;
    
  //setta a current_lvt e azzera l'outgoing message
  execution_time(ret_evt->timestamp, ret_evt->who_generated);
  
  __sync_lock_release(&queue_lock);
  
  return 1;
}

int queue_pending_message_size(void)
{
  return _thr_pool->non_commit_size;
}

void queue_destroy(void)
{
  free(_queue.head);
}