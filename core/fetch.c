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

//#include "atomic.h"
#include "nb_calqueue.h"
#include "hpdcs_utils.h"
#include "core.h"
#include "queue.h"
#include "prints.h"
#include "timer.h"


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



//DEBUG CONTROL VARIABLE
bool ctrl_commit 		= true; //FALSE
bool ctrl_unsafe 		= true; //TRUE
bool ctrl_mark_elim 	= true; //TRUE
bool ctrl_del_elim 		= true;
bool ctrl_del_banana 	= true;



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
	LP_state *lp_ptr = LPS[lp_idx];
	msg_t  * bound_ptr = lp_ptr->bound;
	
	if(((unsigned int)event->timestamp) < (event->frame)){
		printf(RED("COMMIT FALLITO ts:%f fr:%u:\n "),event->timestamp,event->frame);
		print_event(event);
		gdb_abort;
	}
	
	if(!is_valid(event)){//DEBUG
		printf(RED("IS NOT VALID "));
		print_event(event);
	}
	
	if(node == NULL){
		node = event->node;
	}
	
	if(node != 0xbadc0de && delete(nbcalqueue, node)){
		if(LPS[lp_idx]->commit_horizon_ts<node->timestamp  //non sono sotto lock, non è sicuro così
			|| (LPS[lp_idx]->commit_horizon_ts==node->timestamp && LPS[lp_idx]->commit_horizon_tb<node->counter) 
		){
				LPS[lp_idx]->commit_horizon_ts = node->timestamp; //time min ← evt.ts
				LPS[lp_idx]->commit_horizon_tb = node->counter;	
		}
		//event->node = 0x5AFE;                //DEBUG
		event->monitor = 0x5AFE;
		event->local_next = bound_ptr;       //DEBUG
		event->local_previous = node;        //DEBUG
		event->previous_seed = lp_ptr->epoch;//DEBUG
		event->deletion_time = CLOCK_READ();
	}
}



#define do_commit_inside_lock_and_goto_next(event,node,lp_idx)	{	commit_event(event,node,lp_idx);	unlock(lp_idx);	goto get_next;	}
#define do_remove_inside_lock_and_goto_next(event,node,lp_idx)	{	delete(nbcalqueue, node);			unlock(lp_idx);	goto get_next;	}
#define do_skip_inside_lock_and_goto_next(lp_idx)				{	add_lp_unsafe_set(lp_idx); 			unlock(lp_idx); goto get_next; 	}

#define return_evt_to_main_loop()								{	assert(event->monitor != 0xBA4A4A);	break; 	}


#define do_commit_outside_lock_and_goto_next(event,node,lp_idx)	{	commit_event(event,node,lp_idx);					goto get_next;	}
#define do_remove_outside_lock_and_goto_next(event,node,lp_idx)	{	delete(nbcalqueue, node);							goto get_next;	}
#define do_skip_outside_lock_and_goto_next(lp_idx)				{	add_lp_unsafe_set(lp_idx); 							goto get_next; 	}




unsigned int fetch_internal(){
	table *h;
	nbc_bucket_node * node, *node_next, *min_node;
	simtime_t ts, min = INFTY, lvt_ts;
	unsigned int lp_idx, bucket, size, tail_counter = 0, lvt_tb, tb, c = 0, curr_evt_state;
	double bucket_width;
	LP_state *lp_ptr;
	msg_t *event, *local_next_evt, *tmp_node, * bound_ptr;
	bool validity, in_past, in_future;
	
	// Get the minimum node from the calendar queue
    if((node = min_node = getMin(nbcalqueue, &h)) == NULL)
		return 0;

	//used by get_next
	bucket_width = h->bucket_width;
	bucket = hash(node->timestamp, bucket_width);
	size = h->size;	
	
	//reset variables
	safe = false;
	clear_lp_unsafe_set;
	current_msg = NULL; //DEBUG
	
	min = min_node->timestamp;
		
    while(node != NULL){	
		if(++c%500==0){	printf("Eventi scorsi in fetch %u\n",c); } //DEBUG
		
		event = (msg_t *)node->payload;		// Event under exploration
		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
		lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
		ts = node->timestamp;				// Timestamp of the event under exploration
		tb = node->counter;					// Timestamp of the event under exploration
		
		//fin qui fuori dal lock ha funzionato per 10 minuti <8,3>, anche se la coda ha superato 500 elementi, diminuendo così la contesa	
		
		validity = is_valid(event);
		bound_ptr = lp_ptr->bound;
		lvt_ts = bound_ptr->timestamp; 
		lvt_tb = bound_ptr->tie_breaker; 
		curr_evt_state = event->state;
		in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));
		safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
			
		///* VALID *///
		if(validity){
			if(curr_evt_state == EXTRACTED){
				if(in_past){
					///* COMMIT *///
					if(safe && event->frame != 0){ 
						if(ctrl_commit){
							do_commit_outside_lock_and_goto_next(event, node, lp_idx);
						}
					}
					///* EXECUTED AND UNSAFE *///
					if(!safe //&& event->frame != 0
					 || event->frame == 0
					 ){ 
						if(ctrl_unsafe){
							do_skip_outside_lock_and_goto_next(lp_idx);
						}
					}
				}
			}
		}
		
		///* NOT VALID *///
		else{
			///* MARK NON VALID NODE *///
			if(ctrl_mark_elim){
				if((curr_evt_state & ELIMINATED) == 0){
					curr_evt_state = __sync_or_and_fetch(&event->state, ELIMINATED);
				}
			}
			///* DELETE ELIMINATED *///
			if(ctrl_del_elim){
				if(curr_evt_state == ELIMINATED){//if(curr_evt_state == ELIMINATED){
					do_remove_outside_lock_and_goto_next(event,node,lp_idx);
				}
			}
			///* DELETE BANANA NODE *///
			if(ctrl_del_banana){
				if(curr_evt_state == ANTI_MSG && event->monitor == 0xba4a4a){
					do_remove_outside_lock_and_goto_next(event,node,lp_idx);
				}
			}
		}

		
		if(tryLock(lp_idx)) {
			
			validity = is_valid(event);
			
			//if(bound_ptr != lp_ptr->bound){
				bound_ptr = lp_ptr->bound;
				lvt_ts = bound_ptr->timestamp; 
				lvt_tb = bound_ptr->tie_breaker;
				in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb)); 
			//}
		
			//safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
			curr_evt_state = event->state;
		
			
	
			if(validity) {
		
				///* PARTE ORIGINARIAMNETE FUORI DAL LOCK
				if(curr_evt_state == EXTRACTED){
					assert(event->frame != 0);
					if(in_past){
						///* COMMIT *///
						if(!ctrl_commit){
							if(safe){ 
								do_commit_inside_lock_and_goto_next(event,node,lp_idx);
							}
						}
						///* EXECUTED AND UNSAFE *///
						if(!ctrl_unsafe){
							if(!safe){ 
								do_skip_inside_lock_and_goto_next(lp_idx);
							}
						}
					}
				}
				///* PARTE ORIGINARIAMENTE SOTTO LOCK
								
				//da qui in poi il bound è congelato ed è nel passato rispetto al mio evento
				/// GET_NEXT_EXECUTED_AND_VALID: INIZIO ///
				local_next_evt = list_next(lp_ptr->bound);
				
				while(local_next_evt != NULL && !is_valid(local_next_evt)) {
					list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
					//list_place_after_given_node_by_content(TID, to_remove_local_evts, local_next_evt, ((rootsim_list *)to_remove_local_evts)->head->data);
					__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);//local_next_evt->state = ANTI_MSG; //
					local_next_evt->monitor = 0xba4a4a;			//non necessario: è un ottimizzazione del che permette che non vengano fatti rollback in più
					delete(nbcalqueue, local_next_evt->node);	//non necessario: è un ottimizzazione del che permette che non vengano fatti rollback in più
					local_next_evt = list_next(lp_ptr->bound);
				}
				
				if( local_next_evt != NULL && 
					(
						local_next_evt->timestamp < ts || 
						(local_next_evt->timestamp == ts && local_next_evt->tie_breaker < node->counter)
					)
				){
					#if DEBUG==1
					if(local_next_evt->monitor == 0x5AFE){ //DEBUG
						printf("[%u]GET_NEXT_AND_VALID: \n",tid); 
						printf("\tevent         : ");print_event(event);
						printf("\tlocal_next_evt: ");print_event(local_next_evt);
						gdb_abort;
					}
					if(local_next_evt->node == 0xbadc0de){ //DEBUG
						printf(RED("1 - BADCODE REACHED state:%u\n"), event->state);
						gdb_abort;
					}
					#endif	
					event = local_next_evt;
					node  = local_next_evt->node;
					ts = event->timestamp;
					safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
				}
				curr_evt_state = event->state;

				//in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb));	
				
				/// GET_NEXT_EXECUTED_AND_VALID: FINE ///
				if(curr_evt_state == 0x0) {
					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){
						//o era eliminato, o era un antimessaggio nel futuro, in ogni caso devo dire che è stato già eseguito (?)
						/* aggiunta 15:41 del 21/01/2018: INIZIO */
						event->monitor = 0xba4a4a;
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					else {
						return_evt_to_main_loop();
					}
				}
				///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
				else if(curr_evt_state == EXTRACTED){
					if(in_past){
						do_skip_inside_lock_and_goto_next(lp_idx);
					}
					else{
						return_evt_to_main_loop();
					}
				}
				else if(curr_evt_state == ANTI_MSG){
					if(!in_past)
						event->monitor = 0xba4a4a;
					if(event->monitor == 0xba4a4a){//DEBUG
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					return_evt_to_main_loop();
				}
				
				else if(curr_evt_state == ELIMINATED){
					event->monitor = 0xde1;
					do_remove_inside_lock_and_goto_next(event,node,lp_idx);
				}
				
			}
			///* NOT VALID *///		
			else {
								
				///* MARK NON VALID NODE *///
				if( (curr_evt_state & ELIMINATED) == 0 ){ 
					curr_evt_state = __sync_or_and_fetch(&(event->state),ELIMINATED);
				}

				///* DELETE ELIMINATED *///
				if(!ctrl_del_elim){
					if(curr_evt_state == ELIMINATED){
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
				}

				///* SHOULD BE DEAD CODE IF !ctrl_del_elim
				if( (curr_evt_state & EXTRACTED) == 0 ){
					do_remove_inside_lock_and_goto_next(event,node,lp_idx);
				}
				
				assert(curr_evt_state == ANTI_MSG);

				///* ANTI-MESSAGE IN THE FUTURE*///
				if(!in_past){// || event->monitor == 0xba4a4a lo abbiamo già fatto prima
					event->monitor = 0xba4a4a;
				}

				///* DELETE BANANA NODE *///
				if(event->monitor == 0xba4a4a){// || event->monitor == 0xba4a4a lo abbiamo già fatto prima
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
				}
				///* ANTI-MESSAGE IN THE PAST*///
				else{
					return_evt_to_main_loop();
				}
			}
		}
		else {
			add_lp_unsafe_set(lp_idx);
		} 
	

get_next:
		// GET NEXT NODE PRECEDURE
		// From this point over the code is to retrieve the next event on the queue
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
 
 
	if(node == 0x5afe){
		printf(RED("NON VA"));
		print_event(event);
	}
    //node->reserved = true;
    // Set the global variables with the selected event to be processed
    // in the thread main loop.
    current_msg =  event;//(msg_t *) node->payload;
	current_msg->node = node;
	current_msg->tie_breaker = node->counter;

#if DEBUG==1
	if(0xbadc0de == node){
		printf(RED("BADCODE REACHED state:%u\n"), event->state);
	}
    if(event->state == EXTRACTED) node->executed++;
    if(node->executed>2){
		//printf("Lo stesso evento è stato eseguito %u volte.\nEvent_pointer:%p Timestamp:%f\n\n",node->executed,node,node->timestamp);
	}
#endif

	//if(is_marked(node->next, DEL))
	//	return 0;

    return 1;
}


void prune_local_queue_with_ts(simtime_t ts){
	msg_t *tmp_node;
	msg_t *current = ((rootsim_list*)to_remove_local_evts)->head;
	while(current!=NULL){
		tmp_node = list_next(current);
		if(current->max_outgoing_ts < ts){
			list_extract_given_node(tid, to_remove_local_evts, current);
			list_place_after_given_node_by_content(tid, freed_local_evts, current, ((rootsim_list*)freed_local_evts)->head);
		}
		current = tmp_node;
	}
}
