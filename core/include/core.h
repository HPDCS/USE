#ifndef __CORE_H
#define __CORE_H

#include "ROOT-Sim.h"


#include <stdbool.h>
#include <math.h>
#include <float.h>


#define MAX_LPs		2048


typedef struct __msg_t
{  
  unsigned int sender_id;
  unsigned int receiver_id;

  simtime_t timestamp;
  simtime_t sender_timestamp;

  void *data;
  unsigned int data_size;  
  
  int type;
    
  unsigned int who_generated;
  
} msg_t;


extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;


/* Total number of cores required for simulation */
extern unsigned int n_cores;
/* Total number of logical processes running in the simulation */
extern unsigned int n_prc_tot;


void init(unsigned int _thread_num);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);

extern void rootsim_error(bool fatal, const char *msg, ...);


#endif
