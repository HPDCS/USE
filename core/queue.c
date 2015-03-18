
/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Estrazione Thread-safe (non lock-free)
 * Inserimenti avvengono in transazioni
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event_type.h"
#include "queue.h" 


#define MAX_SIZE	4096


typedef struct __queue_t 
{
  event_t *head;
  unsigned int size;
  int lock;
  
} queue_t;


queue_t* queue_init(void)
{
  queue_t *list = malloc(sizeof(queue_t));
    
  list->head = malloc(sizeof(event_t) * MAX_SIZE);
  
  if(list->head == 0)
  {
    fprintf(stderr, "queue_init: Out of memory\n");
    abort();
  }
  
  list->size = 0;
  list->lock = 0;
  
  return list;
}

int queue_insert(queue_t *q, event_t *msg)
{
  if(q->size == MAX_SIZE)
   return 0;
  
  memcpy((q->head + q->size), msg, sizeof(event_t));
  q->size++;
    
  return 1;
}

int queue_min(queue_t *q, event_t *ret_evt)
{
  event_t *aux;
  event_t *min = q->head;
  unsigned int i = 1;

  while(__sync_lock_test_and_set(&q->lock, 1))
    while(q->lock);
    
  if(q->size == 0)
  {
    __sync_lock_release(&q->lock);
    return 0;
  }

  while(i < q->size)
  {
    aux = (q->head + i);
    if(aux->timestamp < min->timestamp)
      min = aux;
    i++;
  }
  
  memcpy(ret_evt, min, sizeof(event_t));
  
  if(q->size > 1)
    memcpy(min, (q->head + (q->size - 1)), sizeof(event_t));
  
  q->size--;
  
  __sync_lock_release(&q->lock);
  
  return 1;
}

void queue_destroy(queue_t *q)
{
  free(q->head);
  free(q);
}
