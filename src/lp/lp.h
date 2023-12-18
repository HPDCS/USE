#pragma once
#ifndef __SIMTYPES_H
#define __SIMTYPES_H

#include <float.h>
#include <atomic.h>
#include <list.h>
#include <events.h>
#include <state.h>
#include <numerical/numerical.h>
#include <local_index/local_index_types.h>



#define current_LP_lvt bound->timestamp //used to retrieve the current LVT TODO: usare questo metodo o inserire il campo nella struttura?


typedef struct _LP_state {

	/// Local ID of the thread (used to translate from bound LPs to local id)
	unsigned int 	lid;

	/// Seed to generate pseudo-random values
	seed_type	seed;

	/// Current execution state of the LP
	short unsigned int state;

	/// This variable mainains the current checkpointing interval for the LP
	unsigned int	ckpt_period;

	/// Counts how many events executed from the last checkpoint (to support PSS)
	unsigned int	from_last_ckpt;

	/// If this variable is set, the next invocation to LogState() takes a new state log, independently of the checkpointing interval
	bool		state_log_forced;

	/// The current state base pointer (updated by SetState())
	void 		* current_base_pointer;

	/// Input messages queue
	list(msg_t)	queue_in;

	/// Pointer to the last correctly elaborated event
	/*volatile*/ msg_t		*bound;


	/// Saved states queue
	list(state_t)	queue_states;

	/// Unique identifier within the LP
	unsigned long long	mark;

	/* ADDED FOR PADS 2018 */

	unsigned int num_executed_frames;
	/*volatile*/ unsigned int epoch;
	
	unsigned int until_ongvt;
	unsigned int until_ckpt_recalc;
		
#if DEBUG == 1
	msg_t* 	last_rollback_event;
#endif

	local_index_t local_index;
	unsigned int wt_binding;

	/// Field for computing the interarrival time ema
	simtime_t ema_ti;
	/// Field for computing the granularity per-lp ema
	simtime_t ema_granularity;
	/// Migration score for LP classification
	simtime_t migration_score;

	simtime_t commit_horizon_ts;
	unsigned long long commit_horizon_tb;
	
	unsigned int until_clean_ckp;

	unsigned int numa_node;

	simtime_t ema_silent_event_granularity;
	simtime_t ema_take_snapshot_time;
	double ema_rollback_probability;
	unsigned int consecutive_forward_count;
	unsigned int consecutive_rollbacks_count;

} LP_state;


void LPs_metada_init(void);


#endif
