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
#include <numerical/numerical.h>


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

	unsigned long long num_executed_frames;
	
	/// Execution state
	short unsigned int state;
} state_t;


extern void SetState(void *new_state);
extern bool LogState(unsigned int);
extern void rollback(unsigned int lid, simtime_t destination_time, unsigned int tie_breaker);
extern void clean_checkpoint(unsigned int lid, simtime_t commit_horizon);

#endif /* _STATE_MGNT_H_ */

