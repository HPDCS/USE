#ifndef __QUEUE_H
#define __QUEUE_H


/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */

typedef struct __event_t event_t;


void queue_init(void);

int queue_insert(event_t *msg);

event_t *queue_pre_min(void);

void queue_register_thread(void);

int queue_min(event_t *ret_evt);

int queue_pending_message_size(void);

void queue_deliver_msgs(void);

void queue_destroy(void);



#endif
