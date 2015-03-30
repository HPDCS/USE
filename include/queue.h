#ifndef __QUEUE_H
#define __QUEUE_H


/* Struttura dati "Coda con priorità" per la schedulazione degli eventi (provissoria):
 * Estrazione a costo O(n)
 * Dimensione massima impostata a tempo di compilazione
 * Thread-safe (non lock-free)
 */

typedef struct __msg_t msg_t;


void queue_init(void);

void queue_insert(unsigned int receiver, double timestamp, unsigned int event_type, void *event_content, unsigned int event_size);

double queue_pre_min(void);

void queue_register_thread(void);

int queue_min(void);

int queue_pending_message_size(void);

double queue_deliver_msgs(void);

void queue_destroy(void);


extern __thread msg_t current_msg __attribute__ ((aligned (64)));

#endif
