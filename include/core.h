#ifndef __CORE_H
#define __CORE_H

#include "ROOT-Sim.h"


#include <stdbool.h>
#include <math.h>
#include <float.h>


#include <float.h>

#include <ROOT-Sim.h>

#define MAX_LPs	2048

#define MAX_DATA_SIZE		16
#define THR_POOL_SIZE		3

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)

typedef struct __msg_t
{  
  unsigned int sender_id;
  unsigned int receiver_id;
  simtime_t timestamp;
  int type;
  unsigned int data_size;  
  unsigned char data[MAX_DATA_SIZE];
  
} msg_t;


extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;


/* Total number of cores required for simulation */
extern unsigned int n_cores;
/* Total number of logical processes running in the simulation */
extern unsigned int n_prc_tot;


void init(unsigned int _thread_num, unsigned int);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);

extern void rootsim_error(bool fatal, const char *msg, ...);



extern unsigned int n_prc_tot;

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);

extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;

extern void rootsim_error(bool fatal, const char *msg, ...);

extern void _mkdir(const char *path);

extern int OnGVT(unsigned int me, void *snapshot);
extern void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);

extern void flush(void);

#endif
