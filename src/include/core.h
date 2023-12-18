#ifndef __CORE_H
#define __CORE_H

#include "ROOT-Sim.h"
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <lp/lp.h>
#include <statistics.h>
#include <limits.h>
#include <nb_calqueue.h>
#include <numa.h>



#define UNDEFINED_WT (512)

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)

#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

#define end_sim(lp)			( __sync_fetch_and_or(&sim_ended[lp/64], (1ULL << (lp%64))) )
#define is_end_sim(lp) 		(( sim_ended[lp/64] & (1ULL << (lp%64)) ) >> (lp%64))

#define numa_from_lp(lp)			(LPS[lp]->numa_node)
#define is_lp_on_my_numa_node(lp)	( numa_from_lp(lp) == current_numa_node )	//verifies if the lp is on the same NUMA node of the current thread

#define SIZEOF_ULL					(sizeof(unsigned long long))
#define LP_BIT_MASK_SIZE			((pdes_config.nprocesses/(SIZEOF_ULL*8) + 1)*SIZEOF_ULL*8)
#define LP_ULL_MASK_SIZE			((pdes_config.nprocesses/(SIZEOF_ULL*8) + 1)*SIZEOF_ULL)
#define LP_MASK_SIZE				((pdes_config.nprocesses/(SIZEOF_ULL*8) + 1))


#define LP_STATE_READY				0x00001
#define LP_STATE_ROLLBACK			0x00008
#define LP_STATE_SILENT_EXEC		0x00010
#define LP_STATE_ONGVT				0x00100

#define BLOCKED_STATE				0x01000
#define is_blocked_state(state)	(bool)(state & BLOCKED_STATE)

struct __bucket_node;

extern simulation_configuration pdes_config;
extern __thread simtime_t current_lvt;
extern __thread simtime_t local_gvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;
extern __thread simtime_t commit_horizon_ts;
extern __thread unsigned int commit_horizon_tb;
extern __thread struct __bucket_node *current_node;
extern __thread unsigned int current_numa_node;
extern __thread unsigned int current_cpu;
extern __thread int __event_from;


extern size_t node_size_msg_t;
extern size_t node_size_state_t;

extern unsigned int pdes_configuration;

/* Commit horizon */ //TODO
extern  simtime_t gvt;
/* Average time between consecutive events */
extern simtime_t t_btw_evts;
extern LP_state **LPS;
extern volatile bool stop;

/* Number of numa nodes on the current machine */
extern unsigned int num_numa_nodes;
extern bool numa_available_bool;
extern volatile bool stop_timer;
extern bool sim_error;


//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);
//void init(unsigned int _thread_num, unsigned int);
void executeEvent(unsigned int LP, simtime_t event_ts, unsigned int event_type, void * event_data, unsigned int event_data_size, void * lp_state, bool safety, msg_t * event);

extern void rootsim_error(bool fatal, const char *msg, ...);
extern void _mkdir(const char *path);

extern int OnGVT(unsigned int me, void *snapshot);
extern void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);
extern void ProcessEvent_reverse(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);
extern void check_OnGVT(unsigned int lp_idx, simtime_t, unsigned int);

//DEBUG
extern bool ctrl_commit;	
extern bool ctrl_unsafe;
extern bool ctrl_mark_elim;
extern bool ctrl_del_elim;
extern bool ctrl_del_banana;

#if VERBOSE > 0
extern __thread unsigned int diff_lp;
#endif


static inline int am_i_committer(){
	int enough_cpu = N_CPU >= 16;
    return 
      pdes_config.enable_committer_threads && 
      enough_cpu && 
      (tid == (N_CPU-1) || tid == (N_CPU/2 -1));
}


#endif
