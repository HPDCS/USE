/**
*			Copyright (C) 2008-2017 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file state.h
* @brief State Management subsystem's header
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/

#pragma once
#ifndef _STATE_MGNT_H_
#define _STATE_MGNT_H_


#include <ROOT-Sim.h>
#include <events.h>
#include <list.h>
#include <numerical.h>


/// Checkpointing interval not yet set
#define  INVALID_STATE_SAVING 		0
/// Copy State Saving checkpointing interval
#define  COPY_STATE_SAVING 		1
/// Periodic State Saving checkpointing interval
#define  PERIODIC_STATE_SAVING		2


#define CKPT_RECALC_PERIOD		200
#define CKPT_RECALC_THRESHOLD	10

extern bool processing_silent_execution;


#define lvt(lid) (LPS[lid]->bound != NULL ? LPS[lid]->bound->timestamp : 0.0)

/// Structure for LP's state
typedef struct _state_t {
	/// Simulation time associated with the state log
	simtime_t	lvt;
	/// This is a pointer used to keep track of changes to simulation states via <SetState>()
	void		*base_pointer;
	/// A pointer to the actual log
	void		*log;
	/// This log has been taken after the execution of this event. This is useful to speedup silent execution.
	msg_t		*last_event;

	/* ADDED FOR PADS 2018 */
	seed_type	seed;
	unsigned long long num_executed_frames;
	
	/// Execution state
	short unsigned int state;
} state_t;


extern void ParallelSetState(void *new_state);
extern bool LogState(unsigned int);
extern unsigned long long RestoreState(unsigned int lid, state_t *restore_state);
extern void rollback(unsigned int lid, simtime_t destination_time, unsigned int tie_breaker);
extern state_t *find_time_barrier(int lid,  simtime_t time);
extern state_t *update_time_barrier(unsigned int, state_t *, simtime_t);
extern void clean_queue_states(unsigned int lid, simtime_t new_gvt);
extern void rebuild_state(unsigned int lid, state_t *state_pointer, simtime_t time);
extern void set_checkpoint_period(unsigned int lid, int period);
extern void force_LP_checkpoint(unsigned int lid);
extern unsigned int silent_execution(unsigned int lid, void *state_buffer, msg_t *evt, simtime_t until_ts, unsigned int tie_breaker);
extern void checkpoint_interval_recalculate(unsigned int lid);
extern void clean_checkpoint(unsigned int lid, simtime_t commit_horizon);

#endif /* _STATE_MGNT_H_ */

