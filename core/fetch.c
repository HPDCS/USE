/*****************************************************************************
*
*	This file is part of NBQueue, a lock-free O(1) priority queue.
*
*   Copyright (C) 2015, Romolo Marotta
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/
/*
 * nonblocking_queue.c
 *
 *  Created on: July 13, 2015
 *  Author: Romolo Marotta
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

#include "nb_calqueue.h"
#include "hpdcs_utils.h"
#include "core.h"
#include "queue.h"
#include "prints.h"
#include "timer.h"

#if POSTING==1
#include <posting.h>
#endif

#if IPI_SUPPORT==1
#include <ipi.h>
#endif

#define LOG_DEQUEUE 0
#define LOG_ENQUEUE 0

#define BOOL_CAS_ALE(addr, old, new)  CAS_x86(\
										UNION_CAST(addr, volatile unsigned long long *),\
										UNION_CAST(old,  unsigned long long),\
										UNION_CAST(new,  unsigned long long)\
									  )
									  	
#define BOOL_CAS_GCC(addr, old, new)  __sync_bool_compare_and_swap(\
										(addr),\
										(old),\
										(new)\
									  )

#define VAL_CAS_GCC(addr, old, new)  __sync_val_compare_and_swap(\
										(addr),\
										(old),\
										(new)\
									  )

#define VAL_CAS  VAL_CAS_GCC 
#define BOOL_CAS BOOL_CAS_GCC

#define FETCH_AND_AND 				__sync_fetch_and_and
#define FETCH_AND_OR 				__sync_fetch_and_or

#define ATOMIC_INC					atomic_inc_x86
#define ATOMIC_DEC					atomic_dec_x86

#define VAL (0ULL)
#define DEL (1ULL)
#define INV (2ULL)
#define MOV (3ULL)

#define MASK_PTR 	(-4LL)
#define MASK_MRK 	(3ULL)
#define MASK_DEL 	(-3LL)

#define MAX_UINT 			(0xffffffffU)
#define MASK_EPOCH	(0x00000000ffffffffULL)
#define MASK_CURR	(0xffffffff00000000ULL)


#define REMOVE_DEL		 0
#define REMOVE_DEL_INV	 1

#define is_marked(...) macro_dispatcher(is_marked, __VA_ARGS__)(__VA_ARGS__)
#define is_marked2(w,r) is_marked_2(w,r)
#define is_marked1(w)   is_marked_1(w)
#define is_marked_2(pointer, mask)	( (UNION_CAST(pointer, unsigned long long) & MASK_MRK) == mask )
#define is_marked_1(pointer)		(UNION_CAST(pointer, unsigned long long) & MASK_MRK)
#define get_unmarked(pointer)		(UNION_CAST((UNION_CAST(pointer, unsigned long long) & MASK_PTR), void *))
#define get_marked(pointer, mark)	(UNION_CAST((UNION_CAST(pointer, unsigned long long)|(mark)), void *))
#define get_mark(pointer)			(UNION_CAST((UNION_CAST(pointer, unsigned long long) & MASK_MRK), unsigned long long))

#define ONE_EVT_PER_LP		0
#define ST_BINDING_LP		1
#define FULL_SPECULATIVE	2


#ifndef OPTIMISTIC_MODE
#define OPTIMISTIC_MODE ONE_EVT_PER_LP
#endif


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
/////////////////			 _	 _	 _	 _	      _   __      __		//////////////////
/////////////////			| \	| |	| \	/ \      / \ |  | /| |  |		//////////////////
/////////////////			|_/	|_|	| | '-.  __   _/ |  |  |  ><		//////////////////
/////////////////			|	| |	|_/	\_/      /__ |__|  | |__|		//////////////////
/////////////////														//////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

bool commit_event(msg_t * event, nbc_bucket_node * node, unsigned int lp_idx){
#if DEBUG == 1
	LP_state *lp_ptr = LPS[lp_idx];
	msg_t  * bound_ptr = lp_ptr->bound;

	if(!is_valid(event)){//DEBUG
		printf(RED("IS NOT VALID"));
		print_event(event);
	}
    if(event->frame==0){
        printf("event not executed in commit_event function\n");
    }
#endif

	(void)event;
	
	event->monitor = (void*)0x5AFE;
	
	if(node == NULL){
		return false;
	}
					
	if(node != (void*) 0xbadc0de && delete(nbcalqueue, node)){
		if(LPS[lp_idx]->commit_horizon_ts<node->timestamp  //non sono sotto lock, non è sicuro così
			|| (LPS[lp_idx]->commit_horizon_ts==node->timestamp && LPS[lp_idx]->commit_horizon_tb<node->counter) 
		){
				//these writes are not atomic. so 2 commits for same Lp on different events can enter here and update same Lp's variables
				//so can happen that commit_ts belongs to first event and commit_tb belongs to second event
				//this is a problem with ongvt period
				LPS[lp_idx]->commit_horizon_ts = node->timestamp; //time min ← evt.ts
				LPS[lp_idx]->commit_horizon_tb = node->counter;	
		}
		
	#if DEBUG == 1
		//event->node = 0x5AFE;                //DEBUG
		//assert(__sync_val_compare_and_swap(&event->monitor, 0x0, 0x5AFE) == 0x0);
		//event->monitor = 0x5AFE;
		event->local_next 		= bound_ptr;       //DEBUG
		event->local_previous 	= (void*) node;        //DEBUG
        #if CONSTANT_CHILD_INVALIDATION==1
        event->previous_seed    = get_epoch_of_LP(lp_idx);
        #else
		event->previous_seed 	= lp_ptr->epoch;//DEBUG
        #endif
		event->deletion_time 	= CLOCK_READ();
#endif

		statistics_post_th_data(tid, STAT_EVENT_COMMIT, 1);

		return true;
	}
	return false;
}



#define do_commit_inside_lock_and_goto_next(event,node,lp_idx)			{	commit_event(event,node,lp_idx);\
																			unlock(lp_idx);\
																			goto get_next;	}
		
#define do_remove_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx){\
																			if(current_node_is_valid){\
																				delete(nbcalqueue, node);\
																			}\
																			unlock(lp_idx);\
																			goto get_next;	}
																			
#define do_skip_inside_lock_and_goto_next(lp_idx)						{	add_lp_unsafe_set(lp_idx);\
																			read_new_min = false;\
																			unlock(lp_idx);\
																			goto get_next;	}
		
#define return_evt_to_main_loop()										{	assert(event->monitor != (void*) 0xBA4A4A);\
																			break; 	}
		
#define do_commit_outside_lock_and_goto_next(event,node,lp_idx)			{	commit_event(event,node,lp_idx);\
																			goto get_next;	}
		
#define do_remove_outside_lock_and_goto_next(event,node,lp_idx)			{	delete(nbcalqueue, node);\
																			goto get_next;	}
		
#define do_skip_outside_lock_and_goto_next(lp_idx)						{	add_lp_unsafe_set(lp_idx);\
																			read_new_min = false;\
																			goto get_next; 	}

#define do_remove_removed_outside_lock_and_goto_next(event,node,lp_idx)	{\
																			if(delete(nbcalqueue, node)){\
																				list_node_clean_by_content(event);\
																				list_insert_tail_by_content(to_remove_local_evts, event);\
																			}\
																			goto get_next;	}

#define  do_remove_removed_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx) {\
																			if(current_node_is_valid){\
																				if(delete(nbcalqueue, node)){\
																					list_node_clean_by_content(event);\
																					list_insert_tail_by_content(to_remove_local_evts, event);\
																				}\
																			}\
																			unlock(lp_idx);\
																			goto get_next; }



//#define set_commit_state_as_safe(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0x5afe); 	old;}
//#define set_commit_state_as_banana(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0xba4a4a); 	old;}
//#define set_commit_state_as_delete(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0xde1); 		old;}

#define set_commit_state_as_safe(event)					(	(event)->monitor =  (void*) 0x5afe 		)		
#define set_commit_state_as_banana(event)				(	(event)->monitor =  (void*) 0xBA4A4A  	) 


bool check_if_event_already_handled_inside_lock(msg_t*event){
	simtime_t ts;
	unsigned int tb;
	LP_state *lp_ptr=LPS[event->receiver_id];
	ts = event->timestamp;
	tb = event->tie_breaker;
	msg_t *bound_ptr=lp_ptr->bound;
	bound_ptr = lp_ptr->bound;
	simtime_t lvt_ts = bound_ptr->timestamp; 
	unsigned int lvt_tb = bound_ptr->tie_breaker; 
	bool in_past=(ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));
	unsigned int current_event_state=event->state;
	bool validity=is_valid(event);
	if
	(
		(current_event_state==ELIMINATED)
		|| ((current_event_state==ANTI_MSG) && (event->monitor==(void*) 0xBA4A4A))
		|| (validity && in_past && (current_event_state==EXTRACTED))
	)
		return true;

	return false;
}

#if DEBUG==1
void check_extracted_in_past_outside_lock(msg_t*event){
	unsigned lp_idx=event->receiver_id;
	LP_state*lp_ptr=LPS[lp_idx];
	simtime_t ts=event->timestamp;
	unsigned int tb=event->tie_breaker;

    if(tryLock(lp_idx)>0){
        //recalculate in past and event->frame
        msg_t*temp_bound_ptr = lp_ptr->bound;
        simtime_t temp_lvt_ts = temp_bound_ptr->timestamp; 
        unsigned int temp_lvt_tb = temp_bound_ptr->tie_breaker; 
        bool temp_in_past = (ts < temp_lvt_ts || (ts == temp_lvt_ts && tb <= temp_lvt_tb));
        if(temp_in_past && event->frame==0){
            printf("this event won't be never fetched\n");
            print_event(event);
            gdb_abort;
        }
        unlock(lp_idx);
    }
}
	
#endif
unsigned int fetch_internal(){
	//cristian:local variables node and event:at the end of this function we have node with an event to execute
	//local variable min_node:node that cointains values min(timestamp) and min_tb
	//local variables min and min_tb: needed for calculate safety of an event
	//local variables lvt_ts,lvt_tb:info on timestamp and tie_breaker of current_event under exploitation
	//local variable bound_ptr: last event execute on current_lp
	//global_variables to update:nothing
	//thread_local_storage_variables to update:current_lp,current_msg,current_node,safe

	table *h;
	nbc_bucket_node * node, *node_next, *min_node;
	simtime_t ts, min = INFTY, lvt_ts;
	unsigned int lp_idx, bucket, size, tail_counter = 0, lvt_tb, tb, curr_evt_state, min_tb;
	double bucket_width;
	LP_state *lp_ptr;
	msg_t *event, *local_next_evt, * bound_ptr;
	bool validity, in_past, read_new_min = true, from_get_next_and_valid,current_node_is_valid=true;

#if DEBUG == 1 || REPORT ==1
	unsigned int c = 0;
#endif

#if POSTING==1
	msg_t *priority_message;
    bool from_info_posted=false,posted=false;
#endif//POSTING

	// Get the minimum node from the calendar queue
    if((node = min_node = getMin(nbcalqueue, &h)) == NULL)
		return 0;
		
	//used by get_next
	bucket_width = h->bucket_width;
	bucket = hash(node->timestamp, bucket_width);
	size = h->size;	
	
	//reset variables
	safe = false;
	current_node = NULL;
	clear_lp_unsafe_set;
	#if OPTIMISTIC_MODE == ST_BINDING_LP
		clear_lp_locked_set;
	#endif
		
	current_msg = NULL; //DEBUG
	
	min = min_node->timestamp;
	min_tb = min_node->counter;
		
	if(local_gvt < min){//update local_gvt with a greater value: greater value means a more updated view of calqueue in respect of other threads with smaller local_gvt
		local_gvt = min;
	}
#if DEBUG==1//not present in original version
    else if(local_gvt>min){
        printf("impossible insert node before min node\n");
        gdb_abort;
    }
#endif
	
    while(node != NULL){
		
#if DEBUG == 1 || REPORT ==1
		c++;
#endif

		if(c==MAX_SKIPPED_LP*n_cores){	return 0; } //DEBUG


		from_get_next_and_valid = false;
		current_node_is_valid = true;//if true then current_node can be eliminated 

		#if POSTING==1
		from_info_posted=false;
		#endif

		event = (msg_t *)node->payload;		// Event under exploration
		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
		lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
		ts = node->timestamp;				// Timestamp of the event under exploration
		tb = node->counter;					// Tie_breaker of the event under exploration
		#if DEBUG==1//not present in original version
        if(tb==0){
            printf("event get from calqueue with tie_breaker 0\n");
            print_event(event);
            gdb_abort;
        }
        if(tb!=event->tie_breaker){
        	printf("tie_breaker in node doesn't match with tie_breaker in event\n");
            print_event(event);
            gdb_abort;
        }
        #endif
		if(read_new_min){
			min_node = node;
			min = min_node->timestamp;
			min_tb = min_node->counter;
		}
		
		//fin qui fuori dal lock ha funzionato per 10 minuti <8,3>, anche se la coda ha superato 500 elementi, diminuendo così la contesa	
		
		validity = is_valid(event);
		bound_ptr = lp_ptr->bound;
		lvt_ts = bound_ptr->timestamp; 
		lvt_tb = bound_ptr->tie_breaker; 
		curr_evt_state = event->state;
		in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));
		safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
		
		///* VALID *///
		if(validity){
			if(curr_evt_state == EXTRACTED && in_past){
                #if DEBUG==1//not present in original version
                check_extracted_in_past_outside_lock(event);
                #endif
			//cristian:valid event extracted and in past,or now it is in "executed" state or eventually will be executed from another thread
				///* COMMIT *///
				if(safe && event->frame != 0){//cristian:this event is safe and executed,can be committed
				#if DEBUG == 1
					event->gvt_on_commit = min;
					event->event_on_gvt_on_commit = (msg_t *)min_node->payload;
				#endif
					do_commit_outside_lock_and_goto_next(event, node, lp_idx);
				}
				///* EXECUTED AND UNSAFE *///
				//cristian:it can be in state "not executed" or(not exclusive) "not safe", ignore it
				else{
					do_skip_outside_lock_and_goto_next(lp_idx);
				}
			}
		}
		
		///* NOT VALID *///
		else{
			///* MARK NON VALID NODE *///
			if((curr_evt_state & ELIMINATED) == 0){//cristian:state "new_evt" or(exclusive) "extracted"
				curr_evt_state = __sync_or_and_fetch(&event->state, ELIMINATED);//cristian:"eliminated" or(exclusive) "anti_msg"
			}
			///* DELETE ELIMINATED *///
			if(curr_evt_state == ELIMINATED){//cristian:state is eliminated, remove it from calqueue

                do_remove_removed_outside_lock_and_goto_next(event,node,lp_idx);			//<<-ELIMINATED	
            }
			///* DELETE BANANA NODE *///
			if(curr_evt_state == ANTI_MSG && event->monitor == (void*) 0xba4a4a){//cristian:state is anti_msg and it is handled, remove it from calqueue
                //remove only in case event->monitor==banana because concurrently another thread is executing this node and we cannot remove it
				do_remove_outside_lock_and_goto_next(event,node,lp_idx);
			}	
		}
		
		if(
#if OPTIMISTIC_MODE != FULL_SPECULATIVE
	#if OPTIMISTIC_MODE == ONE_EVT_PER_LP
		!is_in_lp_unsafe_set(lp_idx) &&
	#else
		!is_in_lp_locked_set(lp_idx) &&
	#endif
#endif
		tryLock(lp_idx)
		) {
			if(bound_ptr != lp_ptr->bound){//update lvt_ts and lvt_tb if they are been changed
				bound_ptr = lp_ptr->bound;
				lvt_ts = bound_ptr->timestamp; 
				lvt_tb = bound_ptr->tie_breaker;
			}
			in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));//update in_past with respect of current event
			validity = is_valid(event);
			curr_evt_state = event->state;
			safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//calculate safe for each events
			
			#if POSTING==1
            priority_message=LPS[lp_idx]->priority_message;
            #if DEBUG==1
            check_LP_info(lp_idx,priority_message);
            #endif
            if(first_has_greater_ts(event,priority_message)){
            	reset_priority_message(lp_idx,priority_message);
        		if(!check_if_event_already_handled_inside_lock(priority_message))
        		{
        			swap_event_with_priority_message(event,priority_message);
            	}
            	//else continue with current event
            }
			#endif//POSTING

            //now event is taken from calqueue and is not skippable
			if(validity) {
				//da qui in poi il bound è congelato ed è nel passato rispetto al mio evento
				/// GET_NEXT_EXECUTED_AND_VALID: INIZIO ///
				local_next_evt = list_next(lp_ptr->bound);
				
				while(local_next_evt != NULL && !is_valid(local_next_evt)) {
                    list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
					local_next_evt->frame = tid+1;
					list_node_clean_by_content(local_next_evt); //NON DOVREBBE SERVIRE
					list_insert_tail_by_content(to_remove_local_evts, local_next_evt);
					local_next_evt->state = ANTI_MSG; //__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);
					set_commit_state_as_banana(local_next_evt); //non necessario: è un ottimizzazione del che permette che non vengano fatti rollback in più
					local_next_evt = list_next(lp_ptr->bound);
				}

                if( local_next_evt != NULL && 
					(
						local_next_evt->timestamp < ts || 
						(local_next_evt->timestamp == ts && local_next_evt->tie_breaker < tb)
					)
				)
				{
					#if DEBUG==1
					if(local_next_evt->monitor == (void*) 0x5AFE){ //DEBUG
						printf("[%u]GET_NEXT_AND_VALID: \n",tid); 
						printf("\tevent         : ");print_event(event);
						printf("\tlocal_next_evt: ");print_event(local_next_evt);

						gdb_abort;
					}
					#endif
					event = local_next_evt;//cristian:aggiorna event
					from_get_next_and_valid = true;
					current_node_is_valid = false;
					ts = event->timestamp;
					tb = event->tie_breaker;
					//the state of the node is set to EXTRACTED because it is the expected one. 
					//In case it is become ANTI, we lost the update. This is done because we cannot retrieve the relative node.
					curr_evt_state = EXTRACTED;

                    //in past non va ricalcolato,sia se avviene lo swap sia se non avviene la variabile past assume lo stesso valore
					safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min)  && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
                }
                else{
                    //node non viene aggiornato rispetto all'evento corrente,quindi se viene preso un evento EXTRACTED dalla coda locale,
                    //nodo non aggiornato,l'evento cambia ad anti_MSG,viene aggiornato current_state e viene rimosso dalla coda global,ma il nodo scorretto viene rimosso
                    curr_evt_state = event->state;
                }
				/// GET_NEXT_EXECUTED_AND_VALID: FINE ///
				if(curr_evt_state == 0x0) {//cristian:state is new_evt,from_get_next_and_valid is false
					#if DEBUG==1//not present in original version
					if(event->frame!=0){
						printf("NEW_EVT already executed\n");
						gdb_abort;
					}
					if(from_get_next_and_valid){
						printf("NEW_EVT taken from get_next_and_valid\n");
						gdb_abort;
					}
					#endif
					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){//cristian:after sync_or_and_fetch new state is EXTRACTED or(exclusive) ANTI_MSG
						
                        //o era eliminato, o era un antimessaggio nel futuro, in ogni caso devo dire che è stato già eseguito (?)
                        set_commit_state_as_banana(event);
						do_remove_removed_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx); 		//<<-ELIMINATED
                            
					}
					else {//cristian:state is changed to extracted,only this thread with lock can change this state from new_evt to extracted
						//cristian:this event extracted can be in the past or future,but is never been executed so return it to simulation loop
						return_evt_to_main_loop();
					}
				}
				///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
				else if(curr_evt_state == EXTRACTED){//event is not new_evt in fact it is extracted previously from another thread(only lock owner can change state to extracted!!)
					if(in_past){//cristian:this event valid+extracted+in past was executed from another thread,ignore it
						#if DEBUG ==1//not present in original version
						if(from_get_next_and_valid){
							printf("event in past and taken from get_local_next function\n");
							gdb_abort;
						}
						if(event->frame==0){
							printf("event EXTRACTED in past not EXECUTED\n");
							gdb_abort;
						}
						#endif
                        do_skip_inside_lock_and_goto_next(lp_idx);
						
					}
					else{//cristian:this event valid+extracted+in future must be executed,return it to simulation loop
						return_evt_to_main_loop();
					}
				}
				//cristian:2 chance:anti_msg in future, anti_msg in past
				else if(curr_evt_state == ANTI_MSG){//cristian:state is anti_msg,remark:we have the lock,lp's bound is freezed,an event in past/future remain concurrently in past/future
					#if DEBUG==1//not present in original version
					if(from_get_next_and_valid){
							printf("event anti_msg taken from get_local_next function\n");
							gdb_abort;
					}
					#endif//DEBUG
					if(!in_past){//cristian:anti_msg in the future,mark it as handled
						set_commit_state_as_banana(event);
					}

					if(event->monitor == (void*) 0xba4a4a){//cristian:if event in state anti_msg is mark as handled remove it from calqueue
						//cristian:now an event anti_msg in the future is marked as handled
                        do_remove_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx);
					}
					else{//cristian:event anti_msg in past not handled,return it to simulation_loop:
						//cristian:if it is anti_msg,then another thread
						//in the past (in wall clock time) has changed its state to extracted and executed it!!!!
						#if DEBUG ==1
						if(event->frame==0){
							printf("event ANTI_MSG in past not EXECUTED\n");
							gdb_abort;
						}
						#endif//DEBUG
						return_evt_to_main_loop();
					}
				}
				else if(curr_evt_state == ELIMINATED){//cristian:state is eliminated,remove it from calqueue
					#if DEBUG==1
					if(from_get_next_and_valid){
							printf("event anti_msg taken from get_local_next function\n");
							gdb_abort;
					}
					#endif//DEBUG
                    do_remove_removed_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx);			//<<-ELIMINATED
				}
				
			}
			///* NOT VALID *///		
			else {
				#if DEBUG==1//not present in original version
				if(from_get_next_and_valid){
						printf("event not valid taken from get_local_next function\n");
						gdb_abort;
				}
				#endif//DEBUG			
				///* MARK NON VALID NODE *///
				if( (curr_evt_state & ELIMINATED) == 0 ){ //cristian:state is "new_evt" or(exclusive) "extracted"
					//event is invalid but current_state is considered valid, set event_state in order to invalidate itself
					curr_evt_state = __sync_or_and_fetch(&(event->state),ELIMINATED);//cristian:new state is "eliminated" or(exclusive) "anti_msg"
				}

				///* DELETE ELIMINATED *///
				if( (curr_evt_state & EXTRACTED) == 0 ){//cristian:state is "new_evt" or(exclusive) "eliminated", but if it is 
					//cristian:new_evt it was changed to eliminated in the previous branch!!!so the event state is eliminated!!!!
					#if DEBUG==1
					assert(curr_evt_state == ELIMINATED);
					#endif
                    do_remove_removed_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx);					//<<-ELIMINATED
				}
				else{//cristian:the event state is not eliminated,new_evt or extracted,so state event must be anti_msg!!!!
					#if DEBUG==1
					assert(curr_evt_state == ANTI_MSG);
					#endif
					///* ANTI-MESSAGE IN THE FUTURE*///
					if(!in_past){//cristian:anti_msg in the future,mark it as handled
							set_commit_state_as_banana(event);
					}	

					///* DELETE BANANA NODE *///
					if(event->monitor == (void*) 0xba4a4a){//cristian:anti_msg handled,remove it from calqueue
                        do_remove_inside_lock_and_goto_next(current_node_is_valid,event,node,lp_idx);
					}
					///* ANTI-MESSAGE IN THE PAST*///
					else{//cristian:anti_msg in past not handled,return it to simulation loop
						//cristian:if it is anti_msg,then another thread
						//cristian:in the past (in wall clock time) has changed its state to extracted and executed it!!!!
						return_evt_to_main_loop();
					}
				}
			}
		}
		else {//cristian:try lock failed,lock is already locked,add current_lp(lp_idx) to lp_unsafe_set,lp_locked_set
			read_new_min = false;
			add_lp_unsafe_set(lp_idx);

		#if OPTIMISTIC_MODE == ST_BINDING_LP
			add_lp_locked_set(lp_idx);
		#endif
			#if POSTING==1
			if(in_past){
				if(validity){
					posted = post_info_event_valid(event);
					#if IPI_SUPPORT==1
        			if(posted)
            			send_ipi_to_lp(event);
        			#else
        			(void)posted;
        			#endif
				}
				else{
					posted = post_info_event_invalid(event);
					#if IPI_SUPPORT==1
        			if(posted)
            			send_ipi_to_lp(event);
        			#else
        			(void)posted;
        			#endif
				}
			}
			#endif
			//event in future,go to next_node
		}
	
get_next:
		// GET NEXT NODE PRECEDURE
		// From this point over the code is to retrieve the next event on the queue
		//cristian:retrieve next_node from calqueue that is visible to simulation.
		do {
			node_next = node->next;
			if(is_marked(node_next, MOV) || node->replica != NULL) {
					//printf("FETCH DONE 4 TABLE:%p NODE:%p NODE_NEXT:%p NODE_REPLICA:%p TAIL:%p counter:%u\n", h, node,node->next, node->replica, g_tail, node->counter);
					return 0;
				}
			
			do {
				node = get_unmarked(node_next);
				node_next = node->next;
			} while(
				(is_marked(node_next, DEL) || is_marked(node_next, INV) ) ||
				(node->timestamp < bucket * bucket_width )
			);
				
			if( (bucket)*bucket_width <= node->timestamp && node->timestamp < (bucket+1)*bucket_width){
				if(is_marked(node_next, MOV) || node->replica != NULL){
    					//printf("FETCH DONE 3\n");
						return 0;
				}
				break;
			}
			else {
				if(node == g_tail){
					if(++tail_counter >= size){
    					//printf("FETCH DONE 2: SIZE:%u\n", size);
						return 0;
					}
				}
				else {
					tail_counter = 0;
				}
				node = h->array + (++bucket % size);
			}
		} while(1);
    }
 
 	if(node == NULL)
        return 0;

#if POSTING==1
    if(from_get_next_and_valid || from_info_posted){
    	node = NULL;
    }
#else
    if(from_get_next_and_valid)
        node = NULL;
#endif

#if DEBUG == 1
	if(node == (void*)0x5afe){
		printf(RED("NON VA"));
		print_event(event);
	}
#endif
    
#if POSTING==1 && REPORT==1
    update_LP_statistics(from_info_posted,from_get_next_and_valid,lp_idx);
#endif
    // Set the global variables with the selected event to be processed
    // in the thread main loop.
    current_msg = event;//(msg_t *) node->payload;
    current_node = node;//reset this information,current_node must be not used in simulation_loop

#if REPORT == 1
	statistics_post_th_data(tid, STAT_GET_NEXT_FETCH, (double)c);
#endif
    return 1;
}

void prune_local_queue_with_ts_old(simtime_t ts){
	msg_t *current = NULL;
	msg_t *tmp_node = NULL;
	
	if(((rootsim_list*)to_remove_local_evts_old)->head != NULL){
		current = (msg_t*) ((rootsim_list*)to_remove_local_evts_old)->head->data;
	}
	while(current!=NULL){
		tmp_node = list_next(current);
#if DEBUG==1
		assert(list_container_of((current))->prev != (void *)0xDDD);
		assert(list_container_of((current))->next != (void *)0xCCC);
		assert(current->timestamp==0 || current->max_outgoing_ts != 0);
#endif
		if(current->max_outgoing_ts < ts){
			list_extract_given_node(tid, to_remove_local_evts_old, current);
#if DEBUG==1
		
			if(tmp_node != NULL)	
				assert(list_prev((tmp_node)) != current);
#endif
			list_node_clean_by_content(current); 
			current->state = -1;
			current->data_size = tid+1;
			//free(list_container_of(current));
			list_insert_tail_by_content(freed_local_evts, current);
			//list_place_after_given_node_by_content(tid, freed_local_evts, current, ((rootsim_list*)freed_local_evts)->head);
		}
		current = tmp_node;
	}
}


void prune_local_queue_with_ts(simtime_t ts){
	msg_t *current = NULL;
	msg_t *tmp_node = NULL;

	msg_t *from = NULL;
	msg_t *to = NULL;
	
	size_t count_nodes= 0;
	

	if(((rootsim_list*)to_remove_local_evts_old)->head != NULL){
		current = (msg_t*) ((rootsim_list*)to_remove_local_evts_old)->head->data;
	}
	while(current!=NULL){
		tmp_node = list_next(current);
		
		if(current->max_outgoing_ts < ts){
            #if POSTING==1
            if(current->posted==POSTED_VALID || current->posted==POSTED_INVALID){
            	if(LPS[current->receiver_id]->priority_message==current){
            		current = tmp_node;
            		continue;
            	}
            	else if (current->collectionable==false){
            		current->collectionable=true;
            		current = tmp_node;
            		continue;
            	}
            	//current is not posted on LP and it is collectionable,collect it!
            }
            //current is never been posted on LP collect it
            #endif
			from = current;
			//trovato il primo nodo valido, continuo localmente la ricerca
			while(current!=NULL && current->max_outgoing_ts < ts){
				#if POSTING==1
            if(current->posted==POSTED_VALID || current->posted==POSTED_INVALID){
            	if(LPS[current->receiver_id]->priority_message==current){
            		break;
            	}
            	else if (current->collectionable==false){
            		current->collectionable=true;
            		break;
            	}
            	//current is not posted on LP and it is collectionable,collect it!
            }
            //current is never been posted on LP collect it
            #endif
				to = current;
				count_nodes++;
				current = list_next(current); 
			}
			//Stacco dalla vecchia lista
			if(list_prev(from) != NULL){
				list_container_of(list_prev(from))->next = list_container_of(to)->next;
			}else{
				((rootsim_list*)to_remove_local_evts_old)->head = list_container_of(to)->next;
			} 
			if(list_next(to) != NULL){
				list_container_of(list_next(to))->prev = list_container_of(from)->prev;
			}else{
				((rootsim_list*)to_remove_local_evts_old)->tail = list_container_of(from)->prev;
			}
			((rootsim_list*)to_remove_local_evts_old)->size -= count_nodes;
			//Attavvo nella nuova lista
			list_insert_tail_by_contents(freed_local_evts, count_nodes ,from, to);
			tmp_node = list_next(to);
			//resetto i campi
			to = NULL;
			from = NULL;
			count_nodes= 0;
		}
		current = tmp_node;
	}
}

