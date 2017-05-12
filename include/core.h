#ifndef __CORE_H
#define __CORE_H

#include "ROOT-Sim.h"
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <reverse.h>
#include "nb_calqueue.h"

#define MAX_LPs	2048

#define MAX_DATA_SIZE		128
#define THR_POOL_SIZE		128

#define MODE_SAF	1
#define MODE_STM	2

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)

#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

#define end_sim(lp)		( __sync_fetch_and_or(&sim_ended[lp/64], (1ULL << (lp%64))) )
#define is_end_sim(lp) 	( sim_ended[lp/64] & (1ULL << (lp%64)) )

struct __bucket_node;

typedef struct __msg_t
{
  unsigned int sender_id;	//Sednder LP
  unsigned int receiver_id;	//Receiver LP
  simtime_t timestamp;
  int type;
  unsigned int data_size;
  unsigned char data[MAX_DATA_SIZE];
  revwin_t *revwin;			//reverse window to rollback
  struct __bucket_node * node;	//address of the belonging node
} msg_t;


extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;

/* Total number of cores required for simulation */
extern unsigned int n_cores;
/* Total number of logical processes running in the simulation */
extern unsigned int n_prc_tot;
/* Commit horizon */ //TODO
extern  simtime_t gvt;
/* Average time between consecutive events */
extern simtime_t t_btw_evts;


//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);
void init(unsigned int _thread_num, unsigned int);

extern void rootsim_error(bool fatal, const char *msg, ...);
extern void _mkdir(const char *path);

extern int OnGVT(unsigned int me, void *snapshot);
extern void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);
extern void ProcessEvent_reverse(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);

#endif
