#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <stdbool.h>
#include <numerical/numerical.h>
#include <argp.h>         // Provides GNU argp() argument parser


enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};


enum ongvt_mode {OPPORTUNISTIC_ONGVT=0, EVT_PERIODIC_ONGVT=1, MS_PERIODIC_ONGVT=2};



enum log_modes {
INVALID_STATE_SAVING, /// Checkpointing interval not yet set
COPY_STATE_SAVING,    /// Copy State Saving checkpointing interval
PERIODIC_STATE_SAVING		/// Periodic State Saving checkpointing interval
};



typedef struct _simulation_configuration {
	unsigned int ncores;
	unsigned int nprocesses;
	int timeout;
	int observe_period;
	
	unsigned char serial;

	enum log_modes checkpointing; 
	unsigned int ckpt_period;
	unsigned int ckpt_collection_period;
	unsigned int ckpt_autonomic_period;
	unsigned int ckpt_autoperiod_bound;

	unsigned int ongvt_period;
	enum ongvt_mode ongvt_mode;

	unsigned char distributed_fetch;
	char   df_bound;
	
	unsigned char numa_rebalance;
	unsigned char enable_mbind;
	unsigned char enable_custom_alloc;
	unsigned char segment_shift;
	unsigned char enable_committer_threads;

	unsigned char enforce_locality;  /// enables pipes usage for increasing cache exploitation
	unsigned char el_dynamic_window;  /// enables pipes usage for increasing cache exploitation
	unsigned char el_locked_pipe_size;
	unsigned char el_evicted_pipe_size;
	double el_window_size;  /// sets the static window size when using pipes
	double el_th_trigger;  /// sets the static window size when using pipes
	double el_roll_th_trigger;
	unsigned char th_below_threashold_cnt;

#ifdef HAVE_PREEMPTION
	bool disable_preemption;	/// If compiled for preemptive Time Warp, it can be disabled at runtime
#endif
	char *output_dir;		/// Destination Directory of output files
} simulation_configuration;

extern simulation_configuration pdes_config;
extern void parse_options(int argn, char **argv);
extern void print_config(void);

#endif