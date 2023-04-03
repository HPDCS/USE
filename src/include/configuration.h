#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <stdbool.h>
#include <numerical.h>


enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};

typedef struct _simulation_configuration {
	unsigned int ncores;
	unsigned int nprocesses;
	unsigned int timeout;
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
	char *output_dir;		/// Destination Directory of output files
} simulation_configuration;

extern simulation_configuration pdes_config;


#endif