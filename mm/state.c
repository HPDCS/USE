/**
*                       Copyright (C) 2008-2017 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file state.c
* @brief The state module is responsible for managing LPs' simulation states.
*	In particular, it allows to take a snapshot, to restore a previous snapshot,
*	and to silently re-execute a portion of simulation events to bring
*	a LP to a partiuclar LVT value for which no simulation state
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*/


#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <core.h>
#include <timer.h>
#include <list.h>
//#include <scheduler/process.h>
//#include <scheduler/scheduler.h>
#include <state.h>
//#include <communication/communication.h>
#include <dymelor.h>
#include <statistics.h>
#include <queue.h>



/// Function pointer to switch between the parallel and serial version of SetState
//void (*SetState)(void *new_state);
//
//void send_antimessages(unsigned int lid, simtime_t lvt) {
//
//}
//
//void activate_LP(unsigned int lp, simtime_t lvt, msg_t *evt, void *state) {		
//	executeEvent(lp, lvt, evt->type, evt->data, evt->data_size, state, true, evt);	
//}

/**
* This function is used to create a state log to be added to the LP's log chain
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The Light Process Identifier
*/
bool LogState(unsigned int lid) {

	bool take_snapshot = false;
	state_t new_state; // If inserted, list API makes a copy of this

	if(is_blocked_state(LPS[lid]->state)) {
		return take_snapshot;
	}

	// Keep track of the invocations to LogState
	LPS[lid]->from_last_ckpt++;

	if(LPS[lid]->state_log_forced) {
		LPS[lid]->state_log_forced = false;
		LPS[lid]->from_last_ckpt = 0;

		take_snapshot = true;
		goto skip_switch;
	}

	// Switch on the checkpointing mode.
	switch(rootsim_config.checkpointing) {

		case COPY_STATE_SAVING:
			take_snapshot = true;
			break;

		case PERIODIC_STATE_SAVING:
			if(LPS[lid]->from_last_ckpt >= LPS[lid]->ckpt_period) {
				take_snapshot = true;
				LPS[lid]->from_last_ckpt = 0;
			}
			break;

		default:
			rootsim_error(true, "State saving mode not supported.");
	}

skip_switch:

	// Shall we take a log?
	if (take_snapshot) {

		// Take a log and set the associated LVT
		//printf("%s %d %d\n", "BANANA", lid, LPS[lid]->from_last_ckpt);
		new_state.log = log_state(lid);
		new_state.lvt = lvt(lid);
		new_state.last_event = LPS[lid]->bound;
		new_state.state = LPS[lid]->state;

		new_state.seed 					= LPS[lid]->seed 					;
		new_state.num_executed_frames	= LPS[lid]->num_executed_frames		;

		// We take as the buffer state the last one associated with a SetState() call, if any
		new_state.base_pointer = LPS[lid]->current_base_pointer;

		// list_insert() makes a copy of the payload
		(void)list_insert_tail(lid, LPS[lid]->queue_states, &new_state);

	}

	return take_snapshot;
}


unsigned long long RestoreState(unsigned int lid, state_t *restore_state) {
	log_restore(lid, restore_state);
	LPS[lid]->current_base_pointer 	= restore_state->base_pointer 			;
	LPS[lid]->state 				= restore_state->state 					;
	return restore_state->num_executed_frames	;
}


/**
* This function bring the state pointed by "state" to "final time" by re-executing all the events without sending any messages
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The id of the Light Process
* @param state_buffer The simulation state to be passed to the LP
* @param pointer The pointer to the element of the input event queue from which start the re-execution
* @param final_time The time where align the re-execution
*
* @return The number of events re-processed during the silent execution
*/
unsigned int silent_execution(unsigned int lid, void *state_buffer, msg_t *evt, simtime_t until_ts, unsigned int tie_breaker) {
	unsigned int events = 0;
	unsigned short int old_state;
	LP_state *lp_ptr = LPS[lid];
	msg_t * local_next_evt, * last_executed_event;
	// current state can be either idle READY, or ROLLBACK, so we save it and then put it back in place
	old_state = LPS[lid]->state;
	LPS[lid]->state = LP_STATE_SILENT_EXEC;

	// Reprocess events. Outgoing messages are explicitly discarded, as this part of
	// the simulation has been already executed at least once
	
	last_executed_event = evt;

	while(1){
		local_next_evt = list_next(evt);

		while(local_next_evt != NULL && local_next_evt != LPS[lid]->bound && !is_valid(local_next_evt)){
			list_extract_given_node(lid, lp_ptr->queue_in, local_next_evt);
			//list_place_after_given_node_by_content(TID, to_remove_local_evts, 
			//									local_next_evt, ((rootsim_list *)to_remove_local_evts)->head->data);
			local_next_evt->state = ANTI_EVENT; //DEBUG
			local_next_evt = list_next(evt);
			// TODO: valutare cancellazione da coda globale
		}

		evt = local_next_evt;
		if(old_state != LP_STATE_ONGVT){
			//if(evt == NULL | evt->node != 0x5A7E)
			if(evt == NULL || evt->timestamp > until_ts || (evt->timestamp == until_ts && evt->tie_breaker >= tie_breaker)  ) 
				break;
		}
		else{
			if(evt == NULL || evt->timestamp > until_ts || (evt->timestamp == until_ts && evt->tie_breaker > tie_breaker)  ) 
				break;
		}
		
		
		events++;
		//activate_LP(lid, evt->timestamp, evt, state_buffer);
		//printf("%d- SILENT_EXECUTE: %f....\n", lid, evt->timestamp);
		executeEvent(lid, evt->timestamp, evt->type, evt->data, evt->data_size, state_buffer, true, evt);
		//printf("%d- SILENT_EXECUTE: %f....DONE\n", lid, evt->timestamp);
		last_executed_event = evt;
	}
	if(old_state != LP_STATE_ONGVT){
		LPS[lid]->bound = last_executed_event;
	}
	
	LPS[lid]->state = old_state;
	return events;
}


/**
* This function rolls back the execution of a certain LP. The point where the
* execution is rolled back is identified by the timestamp <destination_time,tie_breaker>.
* For a rollback operation to take place, that pointer must be set before calling
* this function.
* If the ts is in the future it restore the state in the future (if available).
* If the ts is INFTY it restore the last event executed (LP->bound) (if available).
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The Logical Process Id
*/
void rollback(unsigned int lid, simtime_t destination_time, unsigned int tie_breaker) {
	state_t *restore_state, *s;
	msg_t *last_restored_event;
	msg_t * bound_next;
	unsigned int reprocessed_events;
	unsigned long long starting_frame;

	// Sanity check
	if(LPS[lid]->state != LP_STATE_ROLLBACK && LPS[lid]->state != LP_STATE_ONGVT) {
		rootsim_error(false, "I'm asked to roll back LP %d's execution, but rollback_bound is not set. Ignoring...\n", lid);
		return;
	}

	//printf("ROLLBACK of LP%lu on time %.5f, %lu\n", lid, destination_time, tie_breaker);

	if(destination_time == INFTY){
		bound_next = list_next(LPS[lid]->bound);
		if(bound_next != NULL){
			destination_time = bound_next->timestamp;
			tie_breaker = bound_next->tie_breaker;
		}
	}

	statistics_post_lp_data(lid, STAT_ROLLBACK, 1.0);

	// Find the state to be restored, and prune the wrongly computed states
	restore_state = list_tail(LPS[lid]->queue_states);

	// It's >= rather than > because we have NOT taken into account simultaneous events YET
	while (restore_state != NULL && restore_state->lvt >= destination_time) { //TODO: aggiungere tie_breaker e mettere solo >
		s = restore_state;
		restore_state = list_prev(restore_state);
		if(LPS[lid]->state == LP_STATE_ROLLBACK){
			log_delete(s->log);
			s->last_event = (void *)0xBABEBEEF;
			list_delete_by_content(lid, LPS[lid]->queue_states, s);
		}
	}
	// Restore the simulation state and correct the state base pointer
	log_restore(lid, restore_state);
	LPS[lid]->current_base_pointer 	= restore_state->base_pointer 			;
	
	last_restored_event = restore_state->last_event;
	reprocessed_events = silent_execution(lid, LPS[lid]->current_base_pointer, last_restored_event, destination_time, tie_breaker);
	statistics_post_lp_data(lid, STAT_SILENT, (double)reprocessed_events);

	//The bound variable is set in silent_execution.
	if(LPS[lid]->state == LP_STATE_ROLLBACK){
		LPS[lid]->num_executed_frames = restore_state->num_executed_frames + reprocessed_events;
		LPS[lid]->epoch++;
	}

	LPS[lid]->state 				= restore_state->state 					;
}




/**
* This function computes the time barrier, namely the first state snapshot
* which is associated with a simulation time <= that the passed simtime
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The light Process Id
* @param simtime The simulation time to be associated with a state barrier
* @return A pointer to the state that represents the time barrier
*/
state_t *find_time_barrier(int lid, simtime_t simtime) {

	state_t *barrier_state;

	if(D_EQUAL(simtime, 0.0)) {
		return list_head(LPS[lid]->queue_states);
	}

	barrier_state = list_tail(LPS[lid]->queue_states);

	// Must point to the state with lvt immediately before the GVT
	while (barrier_state != NULL && barrier_state->lvt >= simtime) {
		barrier_state = list_prev(barrier_state);
  	}
  	if(barrier_state == NULL) {
		barrier_state = list_head(LPS[lid]->queue_states);
	}

/*
	// TODO Search for the first full log before the gvt
	while(true) {
		if(is_incremental(current->log) == false)
			break;
	  	current = list_prev(current);
	}
*/

	return barrier_state;
}

/**
* This function sets the buffer of the current LP's state
*
* @author Francesco Quaglia
*
* @param new_state The new buffer
*
* @todo malloc wrapper
*/
void ParallelSetState(void *new_state) {
	LPS[current_lp]->current_base_pointer = new_state;
}







/**
* This function sets the checkpoint mode
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param ckpt_mode The new checkpoint mode
*/
void set_checkpoint_mode(int ckpt_mode) {
	rootsim_config.checkpointing = ckpt_mode;
}




/**
* This function sets the checkpoint period
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
*
* @param lid The Logical Process Id
* @param period The new checkpoint period
*/
void set_checkpoint_period(unsigned int lid, int period) {
	LPS[lid]->ckpt_period = period;
}


/**
* This function tells the logging subsystem to take a LP state log
* upon the next invocation to <LogState>(), independently of the current
* checkpointing period
*
* @author Alessandro Pellegrini
*
* @param lid The Logical Process Id
* @param period The new checkpoint period
*/
void force_LP_checkpoint(unsigned int lid) {
	LPS[lid]->state_log_forced = true;
}

