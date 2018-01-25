#ifndef __CORE_H
#define __CORE_H

#include "ROOT-Sim.h"
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <reverse.h>
#include <simtypes.h>
#include <statistics.h>
#include <limits.h>
#include <nb_calqueue.h>

#define MAX_LPs	2048

#define THR_POOL_SIZE		128


#define MODE_SAF	1
#define MODE_STM	2

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)

#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

#define end_sim(lp)		( __sync_fetch_and_or(&sim_ended[lp/64], (1ULL << (lp%64))) )
#define start_sim(lp)		( __sync_fetch_and_or(&sim_ended[lp/64], ~(1ULL << (lp%64))) )
#define is_end_sim(lp) 	(( sim_ended[lp/64] & (1ULL << (lp%64)) ) >> (lp%64))


#define SIZEOF_ULL					(sizeof(unsigned long long))
#define LP_BIT_MASK_SIZE			((n_prc_tot/(SIZEOF_ULL*8) + 1)*SIZEOF_ULL*8)
#define LP_ULL_MASK_SIZE			((n_prc_tot/(SIZEOF_ULL*8) + 1)*SIZEOF_ULL)
#define LP_MASK_SIZE				((n_prc_tot/(SIZEOF_ULL*8) + 1))


#define LP_STATE_READY				0x00001
#define LP_STATE_RUNNING			0x00002
#define LP_STATE_RUNNING_ECS		0x00004
#define LP_STATE_ROLLBACK			0x00008
#define LP_STATE_SILENT_EXEC		0x00010
#define LP_STATE_SUSPENDED			0x01010
#define LP_STATE_READY_FOR_SYNCH	0x00011	// This should be a blocked state! Check schedule() and stf()
#define LP_STATE_WAIT_FOR_SYNCH		0x01001
#define LP_STATE_WAIT_FOR_UNBLOCK	0x01002
#define LP_STATE_WAIT_FOR_DATA		0x01004
#define LP_STATE_ONGVT				0x00100

#define BLOCKED_STATE			0x01000
#define is_blocked_state(state)	(bool)(state & BLOCKED_STATE)

#define ONGVT_PERIOD 200

struct __bucket_node;

typedef struct _simulation_configuration {
	char *output_dir;		/// Destination Directory of output files
	int backtrace;			/// Debug mode flag
	int scheduler;			/// Which scheduler to be used
	int gvt_time_period;		/// Wall-Clock time to wait before executiong GVT operations
	int gvt_snapshot_cycles;	/// GVT operations to be executed before rebuilding the state
	int simulation_time;		/// Wall-clock-time based termination predicate
	int lps_distribution;		/// Policy for the LP to Kernel mapping
	int ckpt_mode;			/// Type of checkpointing mode (Synchronous, Semi-Asyncronous, ...)
	int checkpointing;		/// Type of checkpointing scheme (e.g., PSS, CSS, ...)
	int ckpt_period;		/// Number of events to execute before taking a snapshot in PSS (ignored otherwise)
	int snapshot;			/// Type of snapshot (e.g., full, incremental, autonomic, ...)
	int check_termination_mode;	/// Check termination strategy: standard or incremental
	bool blocking_gvt;		/// GVT protocol blocking or not
	bool deterministic_seed;	/// Does not change the seed value config file that will be read during the next runs
	int verbose;			/// Kernel verbose
	enum stat_levels stats;		/// Produce performance statistic file (default STATS_ALL)
	bool serial;			// If the simulation must be run serially
	seed_type set_seed;		/// The master seed to be used in this run
	bool core_binding;		/// Bind threads to specific core ( reduce context switches and cache misses )

#ifdef HAVE_PREEMPTION
	bool disable_preemption;	/// If compiled for preemptive Time Warp, it can be disabled at runtime
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	bool disable_allocator;
#endif
} simulation_configuration;

extern simulation_configuration rootsim_config;
extern __thread simtime_t current_lvt;
extern __thread simtime_t local_gvt;
extern __thread unsigned int current_lp;
extern __thread unsigned int tid;
extern __thread simtime_t commit_horizon_ts;
extern __thread unsigned int commit_horizon_tb;
extern __thread struct __bucket_node *current_node;

/* Total number of cores required for simulation */
extern unsigned int n_cores;
/* Total number of logical processes running in the simulation */
extern unsigned int n_prc_tot;
/* Commit horizon */ //TODO
extern  simtime_t gvt;
/* Average time between consecutive events */
extern simtime_t t_btw_evts;
extern LP_state **LPS;

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);
//void init(unsigned int _thread_num, unsigned int);
void executeEvent(unsigned int LP, simtime_t event_ts, unsigned int event_type, void * event_data, unsigned int event_data_size, void * lp_state, bool safety, msg_t * event);

extern void rootsim_error(bool fatal, const char *msg, ...);
extern void _mkdir(const char *path);

extern int OnGVT(unsigned int me, void *snapshot);
extern void ProcessEvent(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);
extern void ProcessEvent_reverse(unsigned int me, simtime_t now, unsigned int event, void *content, unsigned int size, void *state);
extern void check_OnGVT(unsigned int lp_idx);

//DEBUG
extern bool ctrl_commit;	
extern bool ctrl_unsafe;
extern bool ctrl_mark_elim;
extern bool ctrl_del_elim;
extern bool ctrl_del_banana;
		


#endif
