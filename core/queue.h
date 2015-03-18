#ifndef __QUEUE_H
#define __QUEUE_H


/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */

typedef struct __event_t event_t;
typedef struct __queue_t queue_t;


queue_t* queue_init(void);

int queue_insert(queue_t *q, event_t *msg);

int queue_min(queue_t *q, event_t *ret_evt);

void queue_destroy(queue_t *q);



#endif
