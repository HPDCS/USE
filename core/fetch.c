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

#if ENFORCE_LOCALITY == 1
#include "local_index/local_index.h"
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

bool commit_event(msg_t * event, nbc_bucket_node * node, unsigned int lp_idx){
 #if DEBUG == 1
	LP_state *lp_ptr = LPS[lp_idx];
	msg_t  * bound_ptr = lp_ptr->bound;

	if(!is_valid(event)){//DEBUG
		printf(RED("IS NOT VALID"));
		print_event(event);
	}
 #endif

	(void)event;
	
	event->monitor = (void*)0x5AFE;
	
	if(node == NULL){
		return false;
	}
					
	if(node != (void*) 0xbadc0de && delete(nbcalqueue, node)){
		if(LPS[lp_idx]->commit_horizon_ts<node->timestamp  //it is not in mutual exclusion, could be unsafe (?)
			|| (LPS[lp_idx]->commit_horizon_ts==node->timestamp && LPS[lp_idx]->commit_horizon_tb<node->counter) 
		){
				LPS[lp_idx]->commit_horizon_ts = node->timestamp; //time min ← evt.ts
				LPS[lp_idx]->commit_horizon_tb = node->counter;	
		}
		
 #if DEBUG == 1
		event->local_next 		= bound_ptr;       //DEBUG
		event->local_previous 	= (void*) node;        //DEBUG
		event->previous_seed 	= lp_ptr->epoch;//DEBUG
		event->deletion_time 	= CLOCK_READ();
 #endif

		statistics_post_th_data(tid, STAT_EVENT_COMMIT, 1);

		return true;
	}
	return false;
}



#define do_commit_inside_lock_and_goto_next(event,node,lp_idx)			{	commit_event(event,node,lp_idx);\
																			unlock(lp_idx);	/* goto get_next; */	}
		
#define do_remove_inside_lock_and_goto_next(event,node,lp_idx)			{	delete(nbcalqueue, node);\
																			unlock(lp_idx);	/* goto get_next; */	}
																			
#define do_skip_inside_lock_and_goto_next(lp_idx)						{	add_lp_unsafe_set(lp_idx);\
																			read_new_min = false;\
																			unlock(lp_idx); /* goto get_next; */	}
		
#define return_evt_to_main_loop()										{	assert(event->monitor != (void*) 0xBA4A4A);\
																			break; 	}
		
#define do_commit_outside_lock_and_goto_next(event,node,lp_idx)			{	commit_event(event,node,lp_idx);\
																			goto get_next;	}
		
#define do_remove_outside_lock_and_goto_next(event,node,lp_idx)			{	delete(nbcalqueue, node);\
																			goto get_next;	}
		
#define do_skip_outside_lock_and_goto_next(lp_idx)						{	add_lp_unsafe_set(lp_idx);\
																			read_new_min = false;\
																			goto get_next; 	}

#define do_remove_removed_outside_lock_and_goto_next(event,node,lp_idx)	{	if(delete(nbcalqueue, node)){\
																				list_node_clean_by_content(event);\
																				list_insert_tail_by_content(to_remove_local_evts, event);\
																			}\
																			goto get_next;	}

#define do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx)	{	if(delete(nbcalqueue, node)){\
																				list_node_clean_by_content(event);\
																				list_insert_tail_by_content(to_remove_local_evts, event);\
																			}\
																			unlock(lp_idx);	/* goto get_next; */	}




#define set_commit_state_as_safe(event)					(	(event)->monitor =  (void*) 0x5afe 		)		
#define set_commit_state_as_banana(event)				(	(event)->monitor =  (void*) 0xBA4A4A  	) 
													


__thread unsigned int diff_lp = 0;


static msg_t* get_next_and_valid(LP_state *lp_ptr, msg_t* current){
	msg_t *local_next_evt = list_next(current);
				
	while(local_next_evt != NULL && !is_valid(local_next_evt)) {
		list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
		local_next_evt->frame = tid;
		list_node_clean_by_content(local_next_evt); //SHOULD NOT BE USEFUL
		list_insert_tail_by_content(to_remove_local_evts, local_next_evt);
		local_next_evt->state = ANTI_MSG; //__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);
		set_commit_state_as_banana(local_next_evt); //not required: it is an optimization to reduce the number of rollback
		local_next_evt = list_next(current);
	}
	return local_next_evt; 
}

unsigned int fetch_internal(){
	table *h;
	nbc_bucket_node * node, *node_next, *min_node;
	simtime_t ts, min = INFTY, lvt_ts;
	unsigned int lp_idx, bucket, size, tail_counter = 0, lvt_tb, tb, curr_evt_state, min_tb;
	double bucket_width;
	LP_state *lp_ptr;
	msg_t *event, *local_next_evt, * bound_ptr;
	bool validity, in_past, read_new_min = true, from_get_next_and_valid;
 #if DEBUG == 1 || REPORT ==1
	unsigned int c = 0;
 #endif
	
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
		
	if(local_gvt < min){
		local_gvt = min;
	}
		
    while(node != NULL){
		
 #if DEBUG == 1 || REPORT ==1
		c++;
 #endif
 #if VERBOSE > 0
		diff_lp++;
 #endif
	
		if(c==MAX_SKIPPED_LP*n_cores){	return 0; } //DEBUG


		from_get_next_and_valid = false;

		event = (msg_t *)node->payload;		// Event under exploration
		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
		lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
		ts = node->timestamp;				// Timestamp of the event under exploration
		tb = node->counter;					// Tiebreaker of the event under exploration
		
		if(read_new_min){
			min_node = node;
			min = min_node->timestamp;
			min_tb = min_node->counter;
		}
		
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
				///* COMMIT *///
				if(safe && event->frame != 0){
				#if DEBUG == 1
					event->gvt_on_commit = min;
					event->event_on_gvt_on_commit = (msg_t *)min_node->payload;
				#endif
					do_commit_outside_lock_and_goto_next(event, node, lp_idx);
				}
				///* EXECUTED AND UNSAFE *///
				else{ 
					read_new_min = false;
					do_skip_outside_lock_and_goto_next(lp_idx);
				}
			}
		}
		
		///* NOT VALID *///
		else{
			///* MARK NON VALID NODE *///
			if((curr_evt_state & ELIMINATED) == 0){
				curr_evt_state = __sync_or_and_fetch(&event->state, ELIMINATED);
			}
			///* DELETE ELIMINATED *///
			if(curr_evt_state == ELIMINATED){
				do_remove_removed_outside_lock_and_goto_next(event,node,lp_idx);			//<<-ELIMINATED	
			}
			///* DELETE BANANA NODE *///
			if(curr_evt_state == ANTI_MSG && event->monitor == (void*) 0xba4a4a){
				do_remove_outside_lock_and_goto_next(event,node,lp_idx);
			}
			
		}

 #if VERBOSE > 0
		diff_lp--;
 #endif		
		//read_new_min = false;

		if(
 #if OPTIMISTIC_MODE != FULL_SPECULATIVE
	#if OPTIMISTIC_MODE == ONE_EVT_PER_LP
		!is_in_lp_unsafe_set(lp_idx) &&
	#else
		!is_in_lp_locked_set(lp_idx) &&
	#endif
 #endif
 #if DISTRIBUTED_FETCH == 1
		is_lp_on_my_numa_node(lp_idx) &&
 #endif
		tryLock(lp_idx)
		) {
			
			validity = is_valid(event);
			
			if(bound_ptr != lp_ptr->bound){
				bound_ptr = lp_ptr->bound;
				lvt_ts = bound_ptr->timestamp; 
				lvt_tb = bound_ptr->tie_breaker;
				in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb)); 
			}
		
			curr_evt_state = event->state;
		
			if(validity) {
			 #if ENFORCE_LOCALITY == 1
				process_input_channel(lp_ptr);
			 #endif
				//from this point the bound variable is freezed and of course is in the past
				/// GET_NEXT_EXECUTED_AND_VALID: INIZIO ///
				local_next_evt = get_next_and_valid(lp_ptr, lp_ptr->bound);
				
				if( local_next_evt != NULL && 
					(
						local_next_evt->timestamp < ts || 
						(local_next_evt->timestamp == ts && local_next_evt->tie_breaker < node->counter)
					)
				){
				#if DEBUG==1
					if(local_next_evt->monitor == (void*) 0x5AFE){ //DEBUG
						printf("[%u]GET_NEXT_AND_VALID: \n",tid); 
						printf("\tevent         : ");print_event(event);
						printf("\tlocal_next_evt: ");print_event(local_next_evt);
						gdb_abort;
					}
				#endif	
					event = local_next_evt;
					//node = (void*)0x1;//local_next_evt->node;
					from_get_next_and_valid = true;
					ts = event->timestamp;
					tb = node->counter;
					//the state of the node is set to EXTRACTED because it is the expected one. 
					//In case it is become ANTI, we lost the update. This is done because we cannot retrieve the relative node.
					curr_evt_state = EXTRACTED; 
					safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
				}
				else {
					curr_evt_state = event->state;
				}

				/// GET_NEXT_EXECUTED_AND_VALID: FINE ///
				if(curr_evt_state == 0x0) {
					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){
						//or it was eliminated, or it was and antimessage in the future...in any case it has to be considered as executed (?)
						set_commit_state_as_banana(event);
						do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx); 		//<<-ELIMINATED
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
					if(!in_past){
						set_commit_state_as_banana(event);
					}

					if(event->monitor == (void*) 0xba4a4a){
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					else
						return_evt_to_main_loop();
				}
				else if(curr_evt_state == ELIMINATED){
					do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);			//<<-ELIMINATED
				}
				
			}
			///* NOT VALID *///		
			else {
								
				///* MARK NON VALID NODE *///
				if( (curr_evt_state & ELIMINATED) == 0 ){ 
					curr_evt_state = __sync_or_and_fetch(&(event->state),ELIMINATED);
				}

				///* DELETE ELIMINATED *///
				if( (curr_evt_state & EXTRACTED) == 0 ){
					do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);					//<<-ELIMINATED
				}
				else{
			#if DEBUG==1
					assert(curr_evt_state == ANTI_MSG);
			#endif
					///* ANTI-MESSAGE IN THE FUTURE*///
					if(!in_past){
							set_commit_state_as_banana(event);
					}	

					///* DELETE BANANA NODE *///
					if(event->monitor == (void*) 0xba4a4a){
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					///* ANTI-MESSAGE IN THE PAST*///
					else{
						return_evt_to_main_loop();
					}
				}
			}
		}
		else {
			read_new_min = false;
			add_lp_unsafe_set(lp_idx);

		#if OPTIMISTIC_MODE == ST_BINDING_LP
			add_lp_locked_set(lp_idx);
		#endif
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
       
 	if(from_get_next_and_valid)
        node = NULL;
 
 #if DEBUG == 1
	if(node == (void*)0x5afe){
		printf(RED("NON VA"));
		print_event(event);
	}
 #endif
    // Set the global variables with the selected event to be processed
    // in the thread main loop.
    current_msg =  event;//(msg_t *) node->payload;
	current_node = node;
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
			from = current;
			//found the first valid node, the research is continued locally
			while(current!=NULL && current->max_outgoing_ts < ts){
				to = current;
				count_nodes++;
				current = list_next(current); 
			}
			//removing the node from the old list
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
			//attaching the node in the new list
			list_insert_tail_by_contents(freed_local_evts, count_nodes ,from, to);
			tmp_node = list_next(to);
			//resetting local variables
			to = NULL;
			from = NULL;
			count_nodes= 0;
		}
		current = tmp_node;
	}
}

