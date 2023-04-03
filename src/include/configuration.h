#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <stdbool.h>
#include <numerical.h>


enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};


enum log_modes {
INVALID_STATE_SAVING, /// Checkpointing interval not yet set
COPY_STATE_SAVING,    /// Copy State Saving checkpointing interval
PERIODIC_STATE_SAVING		/// Periodic State Saving checkpointing interval
};



typedef struct _simulation_configuration {
	unsigned int ncores;
	unsigned int nprocesses;
	unsigned int timeout;
	
	unsigned char serial;

	enum log_modes checkpointing; 
	unsigned int ckpt_period;
	unsigned int ckpt_collection_period;
	unsigned char enforce_locality;  /// enables pipes usage for increasing cache exploitation
	unsigned char el_dynamic_window;  /// enables pipes usage for increasing cache exploitation
	unsigned char el_locked_pipe_size;
	unsigned char el_evicted_pipe_size;
	double el_window_size;  /// sets the static window size when using pipes
	

#ifdef HAVE_PREEMPTION
	bool disable_preemption;	/// If compiled for preemptive Time Warp, it can be disabled at runtime
#endif

#ifdef HAVE_PARALLEL_ALLOCATOR
	bool disable_allocator;
#endif
	char *output_dir;		/// Destination Directory of output files
} simulation_configuration;

extern simulation_configuration pdes_config;
extern void parse_options(int argn, char **argv);
extern void print_config(void);

#endif