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
#include <state.h>
#include <lp/mm/allocator/dymelor.h>
#include <statistics.h>
#include <queue.h>
#include <hpdcs_utils.h>
#include <prints.h>
#include <state_swapping.h>
#include "mm/checkpoints/autockpt.h"



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
	switch(pdes_config.checkpointing) {

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

		new_state.num_executed_frames	= LPS[lid]->num_executed_frames		;

		// We take as the buffer state the last one associated with a SetState() call, if any
		new_state.base_pointer = LPS[lid]->current_base_pointer;

		// list_insert() makes a copy of the payload
		(void)list_insert_tail(lid, LPS[lid]->queue_states, &new_state);

	}

	return take_snapshot;
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
	//LP_state *lp_ptr = LPS[lid];
	msg_t * local_next_evt, * last_executed_event;
	// current state can be either idle READY, or ROLLBACK, so we save it and then put it back in place
	old_state = LPS[lid]->state;
	LPS[lid]->state = LP_STATE_SILENT_EXEC;
	bool stop_silent = false;


	// Reprocess events. Outgoing messages are explicitly discarded, as this part of
	// the simulation has been already executed at least once
	
	last_executed_event = evt;

	while(1){
		local_next_evt = list_next(evt);


		//while(local_next_evt != NULL && !is_valid(local_next_evt) && local_next_evt != LPS[lid]->bound){
		//	list_extract_given_node(lid, lp_ptr->queue_in, local_next_evt);
		//	//list_place_after_given_node_by_content(TID, to_remove_local_evts, 
		//	//									local_next_evt, ((rootsim_list *)to_remove_local_evts)->head->data);
		//	__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);//local_next_evt->state = ANTI_MSG; //DEBUG									
		//	local_next_evt = list_next(evt);
		//	stop_silent = true;
		//	// TODO: valutare cancellazione da coda globale
		//}
		//if(local_next_evt != NULL && !is_valid(local_next_evt)){
		//	stop_silent = true;
		//	break;
		//}

		evt = local_next_evt;
		if(old_state != LP_STATE_ONGVT){
			//if(evt == NULL | evt->node != 0x5A7E)
			if(stop_silent || evt == NULL || 
				evt->timestamp > until_ts || (evt->timestamp == until_ts && evt->tie_breaker >= tie_breaker)  ) 
				break;
		}
		else{
			if(stop_silent || evt == NULL || 
			evt->timestamp > until_ts || (evt->timestamp == until_ts && evt->tie_breaker > tie_breaker)  ) 
				break;
		}
		
		
		events++;
		executeEvent(lid, evt->timestamp, evt->type, evt->data, evt->data_size, state_buffer, true, evt);
		last_executed_event = evt;
	}

#if DEBUG==1
		if(old_state == LP_STATE_ONGVT){
			if(stop_silent){
				if(local_next_evt->timestamp < until_ts || 
				(local_next_evt->timestamp == until_ts && local_next_evt->tie_breaker <= tie_breaker)){ 
					printf(RED("Found an invalid event during On_GVT\n")); 
					print_event(local_next_evt);
					gdb_abort;
				}
			}
		}
#endif



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
	unsigned int rollback_lenght;
	//unsigned long long starting_frame;
	//unsigned int avg_events_btw_rollback;

	clock_timer rollback_timer;

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

	clock_timer_start(rollback_timer);

	// Find the state to be restored, and prune the wrongly computed states
	restore_state = list_tail(LPS[lid]->queue_states);
	
	assert(restore_state!=NULL);

	// It's >= rather than > because we have NOT taken into account simultaneous events YET
	while (restore_state != NULL && 
                (  
                    restore_state->lvt > destination_time ||
                    (  restore_state->lvt == destination_time && restore_state->last_event->tie_breaker > tie_breaker ) ||
                    ( (CSR_CONTEXT == 0 || destination_time != INFTY) && !is_valid(restore_state->last_event))
                ) 
        )
        { //TODO: add tie_breaker with > (instead of >=)
		s = restore_state;
		restore_state = list_prev(restore_state);
		if(LPS[lid]->state == LP_STATE_ROLLBACK){
			log_delete(s->log);
			s->last_event = (void *)0xBABEBEEF;
			list_delete_by_content(lid, LPS[lid]->queue_states, s);
		}
	}

	assert(restore_state!=NULL);
	
//	if(restore_state->lvt != restore_state->last_event->timestamp){ //DEBUG
//		printf("The checkpoint is ruined!\n");
//		gdb_abort;
//	}
	
	// Restore the simulation state and correct the state base pointer
	
	log_restore(lid, restore_state);
	LPS[lid]->current_base_pointer 	= restore_state->base_pointer 			;
	
	last_restored_event = restore_state->last_event;
	reprocessed_events = silent_execution(lid, LPS[lid]->current_base_pointer, last_restored_event, destination_time, tie_breaker);
	// THE BOUND HAS BEEN RESTORED BY THE SILENT EXECUTION
	
	if(LPS[lid]->state == LP_STATE_ONGVT || destination_time == INFTY)
		statistics_post_lp_data(lid, STAT_EVENT_SILENT_FOR_GVT, (double)reprocessed_events);
    else{
        statistics_post_lp_data(lid, STAT_EVENT_SILENT, (double)reprocessed_events); 
    }
	//The bound variable is set in silent_execution.
	if(LPS[lid]->state == LP_STATE_ROLLBACK){
		rollback_lenght = LPS[lid]->num_executed_frames; 
		LPS[lid]->num_executed_frames = restore_state->num_executed_frames + reprocessed_events;
		rollback_lenght -= LPS[lid]->num_executed_frames;
		LPS[lid]->epoch++;
		LPS[lid]->from_last_ckpt = reprocessed_events%pdes_config.ckpt_period; // TODO
		// TODO
		//LPS[lid]->from_last_ckpt = ??;

		if(destination_time < INFTY){
            statistics_post_lp_data(lid, STAT_ROLLBACK, 1);
            statistics_post_lp_data(lid, STAT_ROLLBACK_LENGTH, (double)rollback_lenght);
            statistics_post_lp_data(lid, STAT_CLOCK_ROLLBACK, (double)clock_timer_value(rollback_timer));
            autockpt_update_ema_rollback_probability(lid);
        }
    }

	LPS[lid]->state = restore_state->state;
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
void SetState(void *new_state) {
	LPS[current_lp]->current_base_pointer = new_state;
}




void clean_checkpoint(unsigned int lid, simtime_t commit_horizon) {
	state_t *to_state, *tmp_state=NULL;
	msg_t *from_msg, *to_msg=NULL, *tmp_msg=NULL;

	(void) tmp_msg;
	
	from_msg = list_head(LPS[lid]->queue_in);
	to_state = list_tail(LPS[lid]->queue_states);


	assert(to_state!=NULL);

	while (to_state != NULL && (to_state->last_event->timestamp) >= commit_horizon){
		to_state = list_prev(to_state);
	}//to_state is the last checkpoint to keep
	
	if(to_state != NULL){// we need an additional state for OnGvt
		to_state = list_prev(to_state);
	}
	
	if(to_state != NULL){
		to_msg = list_prev(to_state->last_event); //last event to be removed
	
		///Rimozione msg dalla vecchia lista
		((struct rootsim_list*)LPS[lid]->queue_in)->head = list_container_of(to_state->last_event);
		list_container_of(to_state->last_event)->prev = NULL; 
		
	
		to_state = list_prev(to_state); //last checkpoint to be removed
		//if(to_state != NULL)
		//	to_msg = to_state->last_event;
	}

	//Removal states
	while (to_state != NULL){ //TODO: adding tie_breaker and use only > (instead of !=)
		tmp_state = list_prev(to_state);
		log_delete(to_state->log);
		to_state->last_event = (void *)0xBABEBEEF;
		list_delete_by_content(lid, LPS[lid]->queue_states, to_state); //<-should be improved
		to_state = tmp_state;
	}
	
	//while(to_msg != NULL){
	//	tmp_msg = list_prev(to_msg);
	//	list_extract_given_node(tid, ((struct rootsim_list*)LPS[lid]->queue_in), to_msg);
	//	list_node_clean_by_content(to_msg); //it should not be required
	//	list_insert_tail_by_content(to_remove_local_evts, to_msg);
	//	to_msg = tmp_msg;
	//}
	
	
	
	//Adding to the new list
	if(to_msg != NULL){
		//NOTE: in this way the size of the list il ruined
		list_container_of(to_msg)->next = NULL;
		if(((struct rootsim_list*)to_remove_local_evts)->tail != NULL){//if the queue is empty
			((struct rootsim_list*)to_remove_local_evts)->tail->next = list_container_of(from_msg);			
		}
		else{
			((struct rootsim_list*)to_remove_local_evts)->head = list_container_of(from_msg);
		}
		
		list_container_of(from_msg)->prev = ((struct rootsim_list*)to_remove_local_evts)->tail;
		((struct rootsim_list*)to_remove_local_evts)->tail = list_container_of(to_msg);
	}
	
	
	assert(list_tail(LPS[lid]->queue_states)!=NULL);
	
}