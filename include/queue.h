#ifndef __QUEUE_H
#define __QUEUE_H


/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */

typedef struct __msg_t msg_t;


void queue_init(void);

int queue_insert(msg_t *msg);

double queue_pre_min(void);

void queue_register_thread(void);

int queue_min(msg_t *ret_evt);

int queue_pending_message_size(void);

void queue_deliver_msgs(void);

void queue_destroy(void);



#endif
