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

#if IPI==1
#include <time.h>
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
				LPS[lp_idx]->commit_horizon_ts = node->timestamp; //time min ← evt.ts
				LPS[lp_idx]->commit_horizon_tb = node->counter;	
		}
		
	#if DEBUG == 1
		//event->node = 0x5AFE;                //DEBUG
		//assert(__sync_val_compare_and_swap(&event->monitor, 0x0, 0x5AFE) == 0x0);
		//event->monitor = 0x5AFE;
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

#define  do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx)	{	if(delete(nbcalqueue, node)){\
																				list_node_clean_by_content(event);\
																				list_insert_tail_by_content(to_remove_local_evts, event);\
																			}\
																			unlock(lp_idx);	/* goto get_next; */	}



//#define set_commit_state_as_safe(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0x5afe); 	old;}
//#define set_commit_state_as_banana(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0xba4a4a); 	old;}
//#define set_commit_state_as_delete(event)				{	unsigned long long old = __sync_val_compare_and_swap(&((event)->monitor), 0x0, 0xde1); 		old;}

#define set_commit_state_as_safe(event)					(	(event)->monitor =  (void*) 0x5afe 		)		
#define set_commit_state_as_banana(event)				(	(event)->monitor =  (void*) 0xBA4A4A  	) 

#if IPI==1
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
/*void swap_with_best_evt(simtime_t min,simtime_t min_tb){
    //don't swap events seeing only timestamps:minimm timestamp must be relative to an event to be processed
    //hence don't swap if minimum timestamp is relative to an event to be "ignored"
    simtime_t timestamp_reliable,timestamp_unreliable,ts;
    unsigned short tie_breaker_reliable,tie_breaker_unreliable,tb;
    timestamp_reliable=INFTY;//double max
    timestamp_unreliable=INFTY;//double max
    msg_t *best_evt_reliable=LPS[current_lp]->best_evt_reliable;
    msg_t *best_evt_unreliable=LPS[current_lp]->best_evt_unreliable;
    //update timestamps if not NULL
    if(best_evt_reliable!=NULL){
        timestamp_reliable=best_evt_reliable->timestamp;
        tie_breaker_reliable=best_evt_reliable->tie_breaker;
    }
    if(best_evt_unreliable!=NULL){
        timestamp_unreliable=best_evt_unreliable->timestamp;
        tie_breaker_unreliable=best_evt_unreliable->tie_breaker;
    }
    //find min timestamp
    if(timestamp_reliable==timestamp_unreliable && timestamp_reliable==INFTY){
        //nop,both are INFTY, (tie breaker don't care)
    }
    else if( (timestamp_reliable<timestamp_unreliable)
        || ( (timestamp_reliable==timestamp_unreliable) && (tie_breaker_reliable <= tie_breaker_unreliable) ) ){//min is evt_reliable
        if(timestamp_reliable<current_lvt
        || ( (timestamp_reliable==current_lvt) && (tie_breaker_reliable <= current_msg->tie_breaker) ) ){//min is evt_reliable and it is high priority compared to current_evt
            //update current_msg and safe variables
            //current_msg=best_evt_reliable;
            //ts=timestamp_reliable;
            //tb=tie_breaker_reliable;
            //safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(current_lp);
            //reset information
            CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
                        (unsigned long)best_evt_reliable,(unsigned long)NULL);
            LPS[current_lp]->num_times_choosen_best_evt_reliable++;
        }
    }
    else{//min is evt_unreliable
        if( (timestamp_unreliable<current_lvt)
        || ( (timestamp_unreliable==current_lvt) && (tie_breaker_unreliable <= current_msg->tie_breaker) ) ){//min is evt_unreliable and it is high priority compared to current_evt
            //update current_msg and safe variables
            //current_msg=best_evt_unreliable;
            //ts=timestamp_unreliable;
            //tb=tie_breaker_unreliable;
            //safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(current_lp);
            //reset information
            CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
                        (unsigned long)best_evt_unreliable,(unsigned long)NULL);
            LPS[current_lp]->num_times_choosen_best_evt_unreliable++;
        }
    }
    return;
}*/


/*msg_t*swap_with_best_event(simtime_t min,simtime_t min_tb,msg_t*curr_evt,bool from_new_evt_to_extracted_curr_evt,bool must_check_curr_evt_state){
	//assumiamo per il momento che l'informazione postata per LP viene postata solo quando vengono prodotti dei nuovi eventi
	//di un evento specifico,quindi lo stato dell'evento postato inizialmente è NEW_EVT e siccome viene utilizzata solo da lui
	//non può diventare or extracted perché in quel caso l'informazione viene utilizzata e resettata
	//in realtà può accadere:il thread gestisce l'evento dopo aver letto una informazione non associata a lui,
	//e successivamente appare l'informazione dell'evento gestito

//ritorna il nuovo evento da eseguire con lo stato eventualmente modificato o null,ripristina lo stato di tutti gi eventi modificati
	//must_check_curr_evt_state=false means that curr_evt is valid to return as is.
	//restore state of current if other events are best.
	msg_t *best_evt_reliable=LPS[current_lp]->best_evt_reliable;
    msg_t *best_evt_unreliable=LPS[current_lp]->best_evt_unreliable;
    msg_t best_evt;
    bool from_new_evt_to_extracted_evt_reliable=false;
    bool from_new_evt_to_extracted_evt_unreliable=false;
    bool dummy;

    //ALL are NULL ->return NULL
    if(curr_evt==NULL && best_evt_reliable ==NULL){
    	//no infos,return NULL
    	return NULL;
    }

	//only best_evt_reliable is available
    else if(curr_evt==NULL && best_evt_reliable!=NULL){
    	//no need to check if curr_state must be restorered,because curr_evt is NULL
    	#if DEBUG==1
    		if(best_evt_reliable->state==ANTI_MSG || best_evt_reliable->state==EXTRACTED){
    			//in realtà questa situazione può accadere perché un thread ancora prima di leggere l'informazione potrebbe gestire l'evento sulla coda
    			gdb_abort;
    		}
    	#endif
    	//reset information
    	CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
    	//no need to check if curr_state must be restorered,because curr_evt is NULL
    	best_evt=update_evt_state(best_evt_reliable,true,&from_new_evt_to_extracted_evt_reliable);
    	if(best_evt!=NULL)
    		LPS[current_lp]->num_times_choosen_best_evt_reliable++;
    	return best_evt;
    }


    //curr_evt!=NULL
    else if(best_evt_reliable==NULL){//only curr_evt is not NULL -> best_event is curr_evt
    	//no need to check if curr_state must be restorered,because curr_evt is best
    	return update_evt_state(curr_evt,must_check_curr_evt_state,&dummy);
    }




    else{//curr_evt!=NULL and best_evt_reliable !=NULL
    	#if DEBUG==1
    			if(best_evt_reliable->state==ANTI_MSG || best_evt_reliable->state==EXTRACTED){
    				gdb_abort;
    			}
    		#endif
    	if(curr_evt==best_evt_reliable){
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
            //not increment counter,i don't have used information
            //no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state,&dummy);
    	}
    	//curr_evt!=NULL and best_evt_reliable !=NULL and are different events
    	//for now it can be NEW_EVT or ELIMINATED,(in realtà possono essere anche EXTRACTED e ANTI_MSG)

    	if(best_evt_reliable->state==ELIMINATED){
    		//eliminalo dalla coda, è safe farlo qui oppure ignora??
    		//if(delete(nbcalqueue, node)){vedere bene come fare la delete e se è importante il valore dell'if
			//	list_node_clean_by_content(event);
			//	list_insert_tail_by_content(to_remove_local_evts, event);
			//}
			//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
    		//no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state,&dummy);
    		}
    	}
    	//state of best_evt_reliable is not ELIMINATED,is NEW_EVT(actual state can be changed from other threads)
    	if(best_evt_reliable->timestamp > curr_evt->timestamp){//curr has best_timestamp
    		if(!must_check_curr_evt_state)
    			//no need to check if curr_state must be restorered,because curr_evt is best
    			return curr_evt;
    		else{
    			//potrei potenzialmente cambiare entrambi gli stati e solo successivamente scegliere il migliore
    			//ho verificato che non è eliminato,verosimilmente non rimarrà eliminato, in ogni caso per ora scelgo di non usare l'informazione,prendo quella più prioritaria
    			best_evt=update_evt_state(curr_evt,must_check_curr_evt_state,&dummy);
    			if(best_evt!=NULL){
    				LPS[current_lp]->num_times_choosen_best_evt_reliable++;
    				return best_evt;
    			}
    			//ho modificato curr_state ma non è il migliore
    		}
    		
    	}
    	else{//timestamp dell'evento reliable è minore

    		//potrei potenzialmente cambiare entrambi gli stati e solo successivamente scegliere il migliore
    		//ho verificato che non è eliminato,verosimilmente non rimarrà eliminato, in ogni caso per ora scelgo di usare l'informazione,prendo quella più prioritaria
    		//restore curr_state
    		if(from_new_evt_to_extracted_curr_evt){//restore event state to NEW_EVT
    			if(CAS_x86((unsigned long long*)&(curr_evt->state),EXTRACTED,NEW_EVT)==0){//CAS failed,event state is ANTI_MSG (from NEW_EVT)
    			}
    			//now current_evt is restore to NEW_EVT state,ignore it
    		}
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
            LPS[current_lp]->num_times_choosen_best_evt_reliable++;
    		return update_evt_state(best_evt_reliable,true,&from_new_evt_to_extracted_evt_reliable);
    	}
    }
    //never reached
    gdb_abort;
    return NULL;
}*/

/*static inline msg_t*curr_null_reliable_not_null(msg_t*best_evt_reliable,msg_t* best_evt_unreliable){
	msg_t*best_evt;
	bool from_new_evt_to_extracted_evt_reliable;
	//no need to check if curr_state must be restorered,because curr_evt is NULL
#if DEBUG==1
	if(best_evt_reliable->state==ANTI_MSG || best_evt_reliable->state==EXTRACTED){
		//in realtà questa situazione può accadere perché un thread ancora prima di leggere l'informazione potrebbe gestire l'evento sulla coda
		gdb_abort;
	}
#endif

	if(best_evt_unreliable==NULL){//case 010:only best_evt_reliable is not NULL ->best_evt is best_evt_reliable
		//reset information
		//TODO check past and future to handled correctly get next and valid 
		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
        	(unsigned long)best_evt_reliable,(unsigned long)NULL);
		//no need to check if curr_state must be restorered,because curr_evt is NULL

		best_evt=update_evt_state(best_evt_reliable,true,&from_new_evt_to_extracted_evt_reliable);
		if(best_evt!=NULL)//I have used the information
			LPS[current_lp]->num_times_choosen_best_evt_reliable++;
		return best_evt;
	}
	else{//case 011 :best_evt_reliable is not NULL and best_evt_unreliable is not NULL
		#if DEBUG==1
			if(best_evt_reliable==best_evt_unreliable){
			//non può accadere almeno per gli eventi generati a partire da un evento x
				gdb_abort;
		}
		//TODO per ora ritorna NULL
		return NULL;
		//check past and future to handled correctly get next and valid
		#endif
		//best_evt_reliable is not NULL and best_evt_unreliable is not NULL and are different each other
		//TODO se best_reliable ha minor timestamp ed è da eseguire/rieseguire NEW_EVT or EXTRACTED si preferisce utilizzare reliable
		//resetta informazione quando possibile
		//TODO check min timestamp update and go aheah
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}*/

/*static inline msg_t*curr_null_reliable_null_unreliable_not_null(msg_t* best_evt_unreliable){
	msg_t*best_evt;
	bool from_new_evt_to_extracted_evt_unreliable;
	//TODO check past and future to handled correctly get next and valid
	//reset information
	CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
    	(unsigned long)best_evt_unreliable,(unsigned long)NULL);
	//no need to check if curr_state must be restorered,because curr_evt is NULL
	best_evt=update_evt_state(best_evt_unreliable,true,&from_new_evt_to_extracted_evt_unreliable);
	if(best_evt!=NULL)//I have used the information
		LPS[current_lp]->num_times_choosen_best_evt_unreliable++;
		return best_evt;
	else{
		return NULL;
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}*/

/*static inline msg_t*curr_not_null_reliable_null(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*best_evt_unreliable){
	nbc_bucket_node * node;
	msg_t*best_evt;
	if(best_evt_unreliable==NULL){//case 100 only curr_evt is not NULL
		//no need to check if curr_state must be restorered,because curr_evt is best
		return update_evt_state(curr_evt,must_check_curr_evt_state);
    }
	else{//case 101 curr_evt and best_evt_unreliable are not NULL
		if(curr_evt==best_evt_unreliable){
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
            (unsigned long)best_evt_unreliable,(unsigned long)NULL);
            //not increment counter,I don't have used information
            //no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state);
		}
		else{//curr_evt and best_evt_unreliable are not NULL and are different each other
			if( (best_evt_unreliable->timestamp < curr_evt->timestamp)
					|| (( best_evt_unreliable->timestamp == curr_evt->timestamp) && (best_evt_unreliable->tie_breaker <= curr_evt->tie_breaker) ))
			{//no need to check get_next_and_valid because timestamp is smaller than current->timestamp(current->timestamp has already execute get_next_and_valid)
				CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
        		(unsigned long)best_evt_unreliable,(unsigned long)NULL);
				best_evt=update_evt_state(best_evt_unreliable,true);
				if(best_evt!=NULL){
					if(from_new_evt_to_extracted_curr_evt){//check if current_state must be restorered
						if(CAS_x86((unsigned long long*)&(curr_evt->state),EXTRACTED,NEW_EVT)==0){//CAS failed,event state is ANTI_MSG (from NEW_EVT)
							set_commit_state_as_banana(curr_evt);//cristian:ANTI_MSG never executed from state NEW_EVT->mark as handled and remove from calqueue
							node=container_of(&curr_evt,nbc_bucket_node,payload);
							if(delete(nbcalqueue, node)){
								list_node_clean_by_content(curr_evt);
								list_insert_tail_by_content(to_remove_local_evts, curr_evt);
							}
						}
						else{
							//now current_evt is restore to NEW_EVT state,ignore it
						}
					}
					LPS[current_lp]->num_times_choosen_best_evt_unreliable++;
					return best_evt;
				}
				else{//best_evt_unreliable not executable
					best_evt=update_evt_state(curr_evt,must_check_curr_evt_state);
					return best_evt;
				}
			}
			else{//current_evt has better timestamp
				best_evt=update_evt_state(curr_evt,must_check_curr_evt_state);
				if(best_evt!=NULL){
					return best_evt;
				}
				else{//not check reliable info if has greater timestamp
					return NULL;
				}
			}
		}
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}


static inline msg_t*curr_not_null_reliable_not_null(msg_t*curr_evt,bool must_check_curr_evt_state,msg_t*best_evt_reliable,msg_t* best_evt_unreliable){
	if(best_evt_unreliable==NULL){//case 110:curr_evt!=NULL and best_evt_reliable !=NULL and best_unreliable==NULL
		if(curr_evt==best_evt_reliable){
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
            //not increment counter,I don't have used information
            //no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state);
		}
		else{//only curr_evt!=NULL and best_evt_reliable !=NULL and are different each other
			//TODO se entrambi sono reliable preferisci quello con timestamp minore, sennò preferisci quello reliable se curr(unreliable) è da eseguire/rieseguire
		}
	}
	else{//case 111 curr_evt!=NULL,best_evt_reliable !=NULL,best_event_unreliable!=NULL
		#if DEBUG==1
			if(best_evt_reliable->state==ANTI_MSG || best_evt_reliable->state==EXTRACTED){
				gdb_abort;
			}
		#endif
		if(curr_evt==best_evt_reliable){
			//resetta informazioni e riconduciti al caso in cui confronti curr_evt e best_unreliable
		}
		else if(curr_evt==best_evt_unreliable){
			//resetta informazioni e riconduciti al caso in cui confronti curr_evt e best_reliable
		}
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}

msg_t*swap_with_best_eventv2(msg_t*curr_evt,bool from_new_evt_to_extracted_curr_evt,bool must_check_curr_evt_state){
	//assumiamo per il momento che l'informazione postata per LP viene postata solo quando vengono prodotti dei nuovi eventi
	//di un evento specifico,quindi lo stato dell'evento postato inizialmente è NEW_EVT e siccome viene utilizzata solo da lui
	//non può diventare or extracted perché in quel caso l'informazione viene utilizzata e resettata
	//in realtà può accadere:il thread gestisce l'evento dopo aver letto una informazione non associata a lui,
	//e successivamente appare l'informazione dell'evento gestito
	
	//lock must be held
	//must_check_curr_evt_state=false means that curr_evt is valid to return as is.
	//in this function we restore state of the events if must be restorered after changing their states
	//curr_evt !=NULL and curr_evt already checks local_queue(get_local_next_and_valid)
	//update current_lp before call this function!

	msg_t *best_evt_reliable=LPS[current_lp]->best_evt_reliable;
    msg_t *best_evt_unreliable=LPS[current_lp]->best_evt_unreliable;
    
    //curr_evt,reliable,unreliable can be NULL(0) or NOT NULL(1)
    //case 000:curr_evt,reliable,unreliable are NULL
    //case 001:curr_evt,reliable are NULL unreliable is not NULL
    //...
    //...
    //case 111:curr_evt,reliable,unreliable are not NULL

	//all branches are exclusive,every case is handled one time 
	

    #if DEBUG == 1
		if(!haveLock(current_lp)){//DEBUG
			printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
			gdb_abort;
		}
	#endif

    if(curr_evt==NULL){
    	printf("now curr_evt must be not NULL\n");
    	gdb_abort;
    }
    if(curr_evt==NULL && best_evt_reliable ==NULL && best_evt_unreliable==NULL){//case:000
    	//ALL events are NULL ->return NULL
    	return NULL;
    }

    else if(curr_evt==NULL && best_evt_reliable!=NULL){//case 010 or 011
    	//return curr_null_reliable_not_null(best_evt_reliable,best_evt_unreliable);
    }

    else if(curr_evt==NULL){//case 001
    	//return curr_null_reliable_null_unreliable_not_null(best_evt_unreliable);
    }
    //from now curr_evt is not NULL cases left: 100,101,110,111

    //curr_evt!=NULL
    else if(best_evt_reliable==NULL){//case 101 or 100 curr_evt is not NULL and best_evt_reliable is NULL
    	return curr_not_null_reliable_null(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_unreliable);
    }
    else{//case 110 or case 111 curr_evt!=NULL and best_evt_reliable !=NULL
    	return curr_not_null_reliable_not_null(curr_evt,must_check_curr_evt_state,best_evt_reliable,best_evt_unreliable);
    }
    printf("BAD CODE\n");
    //never reached
    gdb_abort;
    return NULL;
}
#endif//IPI
*/

/*static inline msg_t*curr_not_null_reliable_null_unreliable_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*best_evt_unreliable){
	nbc_bucket_node * node;
	msg_t*best_evt;
	if(best_evt_unreliable==NULL){//case 100 only curr_evt is not NULL
		//no need to check if curr_state must be restorered,because curr_evt is best
		return update_evt_state(curr_evt,must_check_curr_evt_state);
    }
	else{//case 101 curr_evt and best_evt_unreliable are not NULL
		if(curr_evt==best_evt_unreliable){
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
            (unsigned long)best_evt_unreliable,(unsigned long)NULL);
            //not increment counter,I don't have used information
            //no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state);
		}
		else{//curr_evt and best_evt_unreliable are not NULL and are different each other
			if( (best_evt_unreliable->timestamp < curr_evt->timestamp)
					|| (( best_evt_unreliable->timestamp == curr_evt->timestamp) && (best_evt_unreliable->tie_breaker <= curr_evt->tie_breaker) ))
			{//no need to check get_next_and_valid because timestamp is smaller than current->timestamp(current->timestamp has already execute get_next_and_valid)
				CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
        		(unsigned long)best_evt_unreliable,(unsigned long)NULL);
				best_evt=update_evt_state(best_evt_unreliable,true);
				if(best_evt!=NULL){
					if(from_new_evt_to_extracted_curr_evt){//check if current_state must be restorered
						if(CAS_x86((unsigned long long*)&(curr_evt->state),EXTRACTED,NEW_EVT)==0){//CAS failed,event state is ANTI_MSG (from NEW_EVT)
							set_commit_state_as_banana(curr_evt);//cristian:ANTI_MSG never executed from state NEW_EVT->mark as handled and remove from calqueue
							node=container_of(&curr_evt,nbc_bucket_node,payload);
							if(delete(nbcalqueue, node)){
								list_node_clean_by_content(curr_evt);
								list_insert_tail_by_content(to_remove_local_evts, curr_evt);
							}
						}
						else{
							//now current_evt is restore to NEW_EVT state,ignore it
						}
					}
					LPS[current_lp]->num_times_choosen_best_evt_unreliable++;
					return best_evt;
				}
				else{//best_evt_unreliable not executable
					best_evt=update_evt_state(curr_evt,must_check_curr_evt_state);
					return best_evt;
				}
			}
			else{//current_evt has better timestamp
				best_evt=update_evt_state(curr_evt,must_check_curr_evt_state);
				if(best_evt!=NULL){
					return best_evt;
				}
				else{//not check reliable info if has greater timestamp
					return NULL;
				}
			}
		}
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}


static inline msg_t*curr_not_null_reliable_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,msg_t*best_evt_reliable,msg_t* best_evt_unreliable){
	if(best_evt_unreliable==NULL){//case 110:curr_evt!=NULL and best_evt_reliable !=NULL and best_unreliable==NULL
		if(curr_evt==best_evt_reliable){
    		//reset information
    		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
            (unsigned long)best_evt_reliable,(unsigned long)NULL);
            //not increment counter,I don't have used information
            //no need to check if curr_state must be restorered,because curr_evt is best
    		return update_evt_state(curr_evt,must_check_curr_evt_state);
		}
		else{//only curr_evt!=NULL and best_evt_reliable !=NULL and are different each other
			//TODO se entrambi sono reliable preferisci quello con timestamp minore, sennò preferisci quello reliable se curr(unreliable) è da eseguire/rieseguire
		}
	}
	else{//case 111 curr_evt!=NULL,best_evt_reliable !=NULL,best_event_unreliable!=NULL
		#if DEBUG==1
			if(best_evt_reliable->state==ANTI_MSG || best_evt_reliable->state==EXTRACTED){
				gdb_abort;
			}
		#endif
		if(curr_evt==best_evt_reliable){
			//resetta informazioni e riconduciti al caso in cui confronti curr_evt e best_unreliable
		}
		else if(curr_evt==best_evt_unreliable){
			//resetta informazioni e riconduciti al caso in cui confronti curr_evt e best_reliable
		}
	}
	printf("BAD CODE\n");
	gdb_abort;
	//never reached
	return NULL;
}*/

msg_t*update_evt_statev3(msg_t*event,bool must_check_evt_state){
	//return NULL if event cannot be handled,and from_new_evt_to_extracted set to false if its state must not be restorered in case of ignoring
	//return !=NULL if event can be handled, and from_new_evt_to_extracted set to true if its state must be restorered in case of ignoring
	//return !=NULL if event can be handled, and from_new_evt_to_extracted set to false if its state must not be restorered in case of ignoring
	
	//return null se evento non eseguibile e non da restorare
	//return !=null se evento eseguibile
	nbc_bucket_node * node;
	simtime_t ts, lvt_ts;
	unsigned int lp_idx, lvt_tb, tb, curr_evt_state;
	LP_state *lp_ptr;
	msg_t * bound_ptr;
	bool validity, in_past;

	if(!must_check_evt_state){//nop to do
		return event;
	}
	else{//taken from fetch_internal try_lock succesful,replace skip/go_next with return NULL and return_main_loop with return event
		//adjust do_remove macro with elimination of node and return NULL 
		validity = is_valid(event);
		node=container_of(&event,nbc_bucket_node,payload);
		lp_idx=event->receiver_id;
		lp_ptr=LPS[lp_idx];
		bound_ptr = lp_ptr->bound;
		ts=event->timestamp;
		tb=event->tie_breaker;
		lvt_ts = bound_ptr->timestamp; 
		lvt_tb = bound_ptr->tie_breaker;
		in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb)); 
		curr_evt_state = event->state;
	
		if(validity) {
	
			/*//da qui in poi il bound è congelato ed è nel passato rispetto al mio evento
			/// GET_NEXT_EXECUTED_AND_VALID: INIZIO ///
			local_next_evt = list_next(lp_ptr->bound);
			
			while(local_next_evt != NULL && !is_valid(local_next_evt)) {
				list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
				local_next_evt->frame = tid;
				list_node_clean_by_content(local_next_evt); //NON DOVREBBE SERVIRE
				list_insert_tail_by_content(to_remove_local_evts, local_next_evt);
				local_next_evt->state = ANTI_MSG; //__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);
				set_commit_state_as_banana(local_next_evt); //non necessario: è un ottimizzazione del che permette che non vengano fatti rollback in più
				local_next_evt = list_next(lp_ptr->bound);
			}
			
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
				event = local_next_evt;//cristian:aggiorna event,andrebbe riaggiornato anche node
				//node = (void*)0x1;//local_next_evt->node;
				from_get_next_and_valid = true;
				ts = event->timestamp;
				//cristian:non va ricalcolato il tb rispetto al local_next_evt che è event???
				safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min)  && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
			}
			//node non viene aggiornato rispetto all'evento corrente,quindi se viene preso un evento EXTRACTED dalla coda locale,
			//nodo non aggiornato,l'evento cambia ad anti_MSG,viene aggiornato current_state e viene rimosso dalla coda global,ma il nodo scorretto viene rimosso
			curr_evt_state = event->state;

			#if IPI==1
			#if DEBUG==1
				if(node->payload!=event){
					printf("invalid node or event\n");
					gdb_abort;
				}
			#endif
			#endif
			/// GET_NEXT_EXECUTED_AND_VALID: FINE ///*/
			if(curr_evt_state == NEW_EVT) {//cristian:state is new_evt
				if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){//cristian:after sync_or_and_fetch new state is EXTRACTED or(exclusive) ANTI_MSG
					//o era eliminato, o era un antimessaggio nel futuro, in ogni caso devo dire che è stato già eseguito (?)
					set_commit_state_as_banana(event);//cristian:ANTI_MSG never executed from state NEW_EVT->mark as handled and remove from calqueue
					//do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx); 		//<<-ELIMINATED
					//da modificare con la funzione creata
					if(delete(nbcalqueue, node)){
						list_node_clean_by_content(event);
						list_insert_tail_by_content(to_remove_local_evts, event);
					}
					return NULL;
				}
				else {//cristian:state is changed to extracted,only this thread with lock can change this state from new_evt to extracted
					/*//cristian:this event extracted can be in the past or future,but is never been executed so return it to simulation loop
					#if IPI==1
					from_new_evt_to_extracted_curr_evt=true;
					#endif
					return_evt_to_main_loop();*/
					return event;
				}
			}
			///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
			else if(curr_evt_state == EXTRACTED){//event is not new_evt in fact it is extracted previously from another thread(only lock owner can change state to extracted!!)
				if(in_past){//cristian:this event valid+extracted+in past was executed from another thread,ignore it
					#if IPI==1

					#if DEBUG ==1
						if(event->frame==0){
							printf("event EXTRACTED in past not EXECUTED\n");
							gdb_abort;
						}
					#endif

					#endif
					//do_skip_inside_lock_and_goto_next(lp_idx);
					return NULL;
				}
				else{//cristian:this event valid+extracted+in future must be executed,return it to simulation loop
					
					#if IPI==1

					#if DEBUG ==1
						if(event->frame==0){
							printf("event EXTRACTED in future never EXECUTED\n");
							gdb_abort;
						}
					#endif

					#endif
					//return_evt_to_main_loop();
					return event;
				}
			}
			//cristian:2 chance:anti_msg in future, anti_msg in past
			else if(curr_evt_state == ANTI_MSG){//cristian:state is anti_msg,remark:we have the lock,lp's bound is freezed,an event in past/future remain concurrently in past/future
				if(!in_past){//cristian:anti_msg in the future,mark it as handled
					set_commit_state_as_banana(event);
				}

				if(event->monitor == (void*) 0xba4a4a){//cristian:if event in state anti_msg is mark as handled remove it from calqueue
					//cristian:now an event anti_msg in the future is marked as handled
					//do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					delete(nbcalqueue, node);
					//TODO da sistemare
					return NULL;
				}
				else{//cristian:event anti_msg in past not handled,return it to simulation_loop:
					//cristian:if it is anti_msg,then another thread
					//in the past (in wall clock time) has changed its state to extracted and executed it!!!!
				 	#if IPI==1

					#if DEBUG ==1
						if(event->frame==0){
							printf("event ANTI_MSG in past not EXECUTED\n");
							gdb_abort;
						}
					#endif

					#endif
					//return_evt_to_main_loop();
					return event;
				}//parentesi aggiunte da cristian insieme al debug==1
			}
			else if(curr_evt_state == ELIMINATED){//cristian:state is eliminated,remove it from calqueue
				//do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);			//<<-ELIMINATED
				//TODO
				if(delete(nbcalqueue, node)){
					list_node_clean_by_content(event);
					list_insert_tail_by_content(to_remove_local_evts, event);
				}
				return NULL;
			}
			
		}
		///* NOT VALID *///		
		else {
							
			///* MARK NON VALID NODE *///
			if( (curr_evt_state & ELIMINATED) == 0 ){ //cristian:state is "new_evt" or(exclusive) "extracted"
				//event is invalid but current_state is considered valid, set event_state in order to invalidate itself
				curr_evt_state = __sync_or_and_fetch(&(event->state),ELIMINATED);//cristian:new state is "eliminated" or(exclusive) "anti_msg"
			}

			///* DELETE ELIMINATED *///
			if( (curr_evt_state & EXTRACTED) == 0 ){//cristian:state is "new_evt" or(exclusive) "eliminated", but if it is 
				//cristian:new_evt it was changed to eliminated in the previous branch!!!so the event state is eliminated!!!!
				//do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);					//<<-ELIMINATED
				//TODO
				if(delete(nbcalqueue, node)){
					list_node_clean_by_content(event);
					list_insert_tail_by_content(to_remove_local_evts, event);
				}
				return NULL;
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
					//do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					//TODO
					delete(nbcalqueue, node);
					return NULL;
				}
				///* ANTI-MESSAGE IN THE PAST*///
				else{//cristian:anti_msg in past not handled,return it to simulation loop
					//cristian:if it is anti_msg,then another thread
					//in the past (in wall clock time) has changed its state to extracted and executed it!!!!
					//return_evt_to_main_loop();
					return event;
				}
			}
		}
	}
	printf("BAD CODE\n");
    //never reached
    gdb_abort;
    return NULL;
	
}

static inline void reset_current_LP_infov3(msg_t*evt,bool reliable){
	if(reliable){
		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_reliable),
    	(unsigned long)evt,(unsigned long)NULL);
    	//not increment counter,I don't have used information
    	//no need to check if curr_state must be restorered,because curr_evt is best
	}
	else{
		CAS_x86((unsigned long long*)&(LPS[current_lp]->best_evt_unreliable),
    	(unsigned long)evt,(unsigned long)NULL);
    	//not increment counter,I don't have used information
    	//no need to check if curr_state must be restorered,because curr_evt is best
	}
	return;
}

static inline void update_statisticsv3(bool reliable){
	if(reliable){
		LPS[current_lp]->num_times_choosen_best_evt_reliable++;
	}
	else{
		LPS[current_lp]->num_times_choosen_best_evt_unreliable++;
	}
	return;
}

static inline msg_t*only_curr_to_handlev3(msg_t*curr_evt,bool must_check_curr_evt_state){
	return update_evt_statev3(curr_evt,must_check_curr_evt_state);
}

static inline msg_t*only_curr_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state){
	return only_curr_to_handlev3(curr_evt,must_check_curr_evt_state);
}

static inline void restore_evt_statev3(msg_t*evt){
	nbc_bucket_node * node;
	if(CAS_x86((unsigned long long*)&(evt->state),EXTRACTED,NEW_EVT)==0){//CAS failed,event state is ANTI_MSG (from NEW_EVT)
		set_commit_state_as_banana(evt);//cristian:ANTI_MSG never executed from state NEW_EVT->mark as handled and remove from calqueue
		node=container_of(&evt,nbc_bucket_node,payload);
		if(delete(nbcalqueue, node)){
			list_node_clean_by_content(evt);
			list_insert_tail_by_content(to_remove_local_evts, evt);
		}
	}
}
static inline msg_t*only_curr_not_fetchable_and_one_evt_to_handlev3(msg_t*curr_evt,msg_t*evt,bool reliable){
	msg_t*best_evt;

	if(curr_evt==evt){
		//reset information
		reset_current_LP_infov3(evt,reliable);
		return NULL;
	}
	else{
		if( (evt->timestamp < curr_evt->timestamp)
				|| (( evt->timestamp == curr_evt->timestamp) && (evt->tie_breaker <= curr_evt->tie_breaker) ))
		{//no need to check get_next_and_valid because timestamp is smaller than current->timestamp(current->timestamp has already execute get_next_and_valid)
			
			reset_current_LP_infov3(evt,reliable);
			best_evt=update_evt_statev3(evt,true);
			if(best_evt!=NULL){
				update_statisticsv3(reliable);
				return best_evt;
			}
			else{//evt not executable,curr is already not fetchable
				return NULL;
			}
		}
		else{//current_evt has better timestamp
			return NULL;
		}
	}

	printf("BAD CODE\n");
    //never reached
    gdb_abort;
    return NULL;
}

static inline msg_t*only_curr_and_one_evt_to_handlev3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*evt,bool reliable){
	msg_t*best_evt;

	if(curr_evt==evt){
		//reset information
		reset_current_LP_infov3(evt,reliable);
		return only_curr_to_handlev3(curr_evt,must_check_curr_evt_state);
	}
	else{
		if( (evt->timestamp < curr_evt->timestamp)
				|| (( evt->timestamp == curr_evt->timestamp) && (evt->tie_breaker <= curr_evt->tie_breaker) ))
		{//no need to check get_next_and_valid because timestamp is smaller than current->timestamp(current->timestamp has already execute get_next_and_valid)
			
			reset_current_LP_infov3(evt,reliable);
			best_evt=update_evt_statev3(evt,true);
			if(best_evt!=NULL){
				if(from_new_evt_to_extracted_curr_evt){//check if current_state must be restorered
					restore_evt_statev3(curr_evt);
				}
				update_statisticsv3(reliable);
				return best_evt;
			}
			else{//evt not executable
				return only_curr_to_handlev3(curr_evt,must_check_curr_evt_state);
			}
		}
		else{//current_evt has better timestamp
			return only_curr_to_handlev3(curr_evt,must_check_curr_evt_state);
		}
	}

	printf("BAD CODE\n");
    //never reached
    gdb_abort;
    return NULL;
}
static inline msg_t*curr_not_null_and_one_evt_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*evt,bool reliable){
	return only_curr_and_one_evt_to_handlev3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,evt,reliable);
}

static inline msg_t*curr_not_null_reliable_not_null_unreliable_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*best_evt_reliable,msg_t*best_evt_unreliable){
	//ALL events are not NULL
	msg_t*best_evt;
	if( (best_evt_reliable->timestamp < best_evt_unreliable->timestamp)
				|| (( best_evt_reliable->timestamp == best_evt_unreliable->timestamp) && (best_evt_reliable->tie_breaker <= best_evt_unreliable->tie_breaker) ))
	{
		best_evt=only_curr_and_one_evt_to_handlev3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_reliable,true);
		if(best_evt!=NULL){
			return best_evt;
		}
		else{//best_evt is NULL,curr is not fetchable
			best_evt=only_curr_not_fetchable_and_one_evt_to_handlev3(curr_evt,best_evt_unreliable,false);
			return best_evt;
		}
	}
	else{
		best_evt=only_curr_and_one_evt_to_handlev3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_unreliable,false);
		if(best_evt!=NULL){
			return best_evt;
		}
		else{//best_evt is NULL,curr is not fetchable
			best_evt=only_curr_not_fetchable_and_one_evt_to_handlev3(curr_evt,best_evt_reliable,true);
			return best_evt;
		}
	}
	return NULL;
}

static inline msg_t*curr_not_null_reliable_null_unreliable_not_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*best_evt_unreliable){
	return curr_not_null_and_one_evt_not_nullv3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_unreliable,false);
}

static inline msg_t*curr_not_null_reliable_not_null_unreliable_nullv3(msg_t*curr_evt,bool must_check_curr_evt_state,bool from_new_evt_to_extracted_curr_evt,msg_t*best_evt_reliable){
	return curr_not_null_and_one_evt_not_nullv3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_reliable,true);
}




msg_t*swap_with_best_eventv3(msg_t*curr_evt,bool from_new_evt_to_extracted_curr_evt,bool must_check_curr_evt_state){
	//assumiamo per il momento che l'informazione postata per LP viene postata solo quando vengono prodotti dei nuovi eventi
	//di un evento specifico,quindi lo stato dell'evento postato inizialmente è NEW_EVT e siccome viene utilizzata solo da lui
	//non può diventare or extracted perché in quel caso l'informazione viene utilizzata e resettata
	//in realtà può accadere:il thread gestisce l'evento dopo aver letto una informazione non associata a lui,
	//e successivamente appare l'informazione dell'evento gestito
	
	//lock must be held
	//must_check_curr_evt_state=false means that curr_evt is valid to return as is.
	//in this function we restore state of the events if must be restorered after changing their states
	//curr_evt !=NULL and curr_evt already checks local_queue(get_local_next_and_valid)
	//update current_lp before call this function!

	msg_t *best_evt_reliable=LPS[current_lp]->best_evt_reliable;
    msg_t *best_evt_unreliable=LPS[current_lp]->best_evt_unreliable;

	//all branches are exclusive,every case is handled one time 
	

    #if DEBUG == 1
		if(!haveLock(current_lp)){//DEBUG
			printf(RED("[%u] Sto operando senza lock: LP:%u LK:%u\n"),tid, current_lp, checkLock(current_lp)-1);
			gdb_abort;
		}
	#endif

    if(curr_evt==NULL){
    	printf("now curr_evt must be not NULL\n");
    	gdb_abort;
    }
    //curr_evt !=NULL
    if(curr_evt!=NULL && best_evt_reliable ==NULL && best_evt_unreliable==NULL){//case:100
    	return only_curr_not_nullv3(curr_evt,must_check_curr_evt_state);
    }
    //cases left: 101,110,111

    else if(best_evt_reliable==NULL){//case 101 curr_evt is not NULL,best_evt_reliable is NULL,best_unreliable not NULL
    	return curr_not_null_reliable_null_unreliable_not_nullv3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_unreliable);
    }
    else{//case 110 or case 111 curr_evt!=NULL and best_evt_reliable !=NULL
    	if(best_evt_unreliable==NULL){//case 110
    		return curr_not_null_reliable_not_null_unreliable_nullv3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_reliable);
    	}
    	else{//case 111
    		return curr_not_null_reliable_not_null_unreliable_not_nullv3(curr_evt,must_check_curr_evt_state,from_new_evt_to_extracted_curr_evt,best_evt_reliable,best_evt_unreliable);
    	}
    }
    printf("BAD CODE\n");
    //never reached
    gdb_abort;
    return NULL;
}
#endif//IPI

unsigned int fetch_internal(){
	//local variables node and event:at the end of this function we have node with an event to execute
	//local variable min_node:node that cointains values min(timestamp) and min_tb
	//local variables min and min_tb: needed for calculate safety of an event
	//local variables:lvt_ts,lvt_tb info on timestamp and tie_breaker of current_event under exploitation
	//local variable bound_ptr last event execute from current_lp

	//global_variables to update:nothing
	//thread_local_storage_variables to update:current_lp,current_msg,current_node,safe

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

#if IPI==1
	bool from_new_evt_to_extracted_curr_evt=false;//updated to true only when lock held,event_valid,state==NEW_EVT
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

		if(c==MAX_SKIPPED_LP*n_cores){	return 0; } //DEBUG


		from_get_next_and_valid = false;

		event = (msg_t *)node->payload;		// Event under exploration
		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
		lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
		ts = node->timestamp;				// Timestamp of the event under exploration
		tb = node->counter;					// Timestamp of the event under exploration
		
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
					read_new_min = false;//questa riga di codice è già embeddata dentro la macro do_skip,rimuoverla per una maggior comprensione
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
				do_remove_outside_lock_and_goto_next(event,node,lp_idx);
			}
			
		}
		
		//read_new_min = false;

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
			
			validity = is_valid(event);
			
			if(bound_ptr != lp_ptr->bound){
				bound_ptr = lp_ptr->bound;
				lvt_ts = bound_ptr->timestamp; 
				lvt_tb = bound_ptr->tie_breaker;
				in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb)); 
			}
		
			curr_evt_state = event->state;
		
			if(validity) {
		
				//da qui in poi il bound è congelato ed è nel passato rispetto al mio evento
				/// GET_NEXT_EXECUTED_AND_VALID: INIZIO ///
				local_next_evt = list_next(lp_ptr->bound);
				
				while(local_next_evt != NULL && !is_valid(local_next_evt)) {
					list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
					local_next_evt->frame = tid;
					list_node_clean_by_content(local_next_evt); //NON DOVREBBE SERVIRE
					list_insert_tail_by_content(to_remove_local_evts, local_next_evt);
					local_next_evt->state = ANTI_MSG; //__sync_or_and_fetch(&local_next_evt->state, ELIMINATED);
					set_commit_state_as_banana(local_next_evt); //non necessario: è un ottimizzazione del che permette che non vengano fatti rollback in più
					local_next_evt = list_next(lp_ptr->bound);
				}
				
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
					event = local_next_evt;//cristian:aggiorna event,andrebbe riaggiornato anche node
					//node = (void*)0x1;//local_next_evt->node;
					from_get_next_and_valid = true;
					ts = event->timestamp;
					//cristian:non va ricalcolato il tb rispetto al local_next_evt che è event???
					safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min)  && (tb <= min_tb))) && !is_in_lp_unsafe_set(lp_idx);//se lo sposto più avanti non funziona!!!
				}
				//node non viene aggiornato rispetto all'evento corrente,quindi se viene preso un evento EXTRACTED dalla coda locale,
				//nodo non aggiornato,l'evento cambia ad anti_MSG,viene aggiornato current_state e viene rimosso dalla coda global,ma il nodo scorretto viene rimosso
				curr_evt_state = event->state;

				/*#if IPI==1
				#if DEBUG==1
					if(node->payload!=event){
						printf("invalid node or event\n");
						gdb_abort;
					}
				#endif
				#endif*/
				/// GET_NEXT_EXECUTED_AND_VALID: FINE ///
				if(curr_evt_state == 0x0) {//cristian:state is new_evt
					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){//cristian:after sync_or_and_fetch new state is EXTRACTED or(exclusive) ANTI_MSG
						//o era eliminato, o era un antimessaggio nel futuro, in ogni caso devo dire che è stato già eseguito (?)
						set_commit_state_as_banana(event);//cristian:ANTI_MSG never executed from state NEW_EVT->mark as handled and remove from calqueue
						do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx); 		//<<-ELIMINATED
					}
					else {//cristian:state is changed to extracted,only this thread with lock can change this state from new_evt to extracted
						//cristian:this event extracted can be in the past or future,but is never been executed so return it to simulation loop
						#if IPI==1
						from_new_evt_to_extracted_curr_evt=true;
						#endif
						return_evt_to_main_loop();
					}
				}
				///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
				else if(curr_evt_state == EXTRACTED){//event is not new_evt in fact it is extracted previously from another thread(only lock owner can change state to extracted!!)
					if(in_past){//cristian:this event valid+extracted+in past was executed from another thread,ignore it
						#if IPI==1

						#if DEBUG ==1
							if(event->frame==0){
								printf("event EXTRACTED in past not EXECUTED\n");
								gdb_abort;
							}
						#endif

						#endif
						do_skip_inside_lock_and_goto_next(lp_idx);
					}
					else{//cristian:this event valid+extracted+in future must be executed,return it to simulation loop
						
						#if IPI==1

						#if DEBUG ==1
							if(event->frame==0){
								printf("event EXTRACTED in future never EXECUTED\n");
								gdb_abort;
							}
						#endif

						#endif
						return_evt_to_main_loop();
						
					}
				}
				//cristian:2 chance:anti_msg in future, anti_msg in past
				else if(curr_evt_state == ANTI_MSG){//cristian:state is anti_msg,remark:we have the lock,lp's bound is freezed,an event in past/future remain concurrently in past/future
					if(!in_past){//cristian:anti_msg in the future,mark it as handled
						set_commit_state_as_banana(event);
					}

					if(event->monitor == (void*) 0xba4a4a){//cristian:if event in state anti_msg is mark as handled remove it from calqueue
						//cristian:now an event anti_msg in the future is marked as handled
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					else{//cristian:event anti_msg in past not handled,return it to simulation_loop:
						//cristian:if it is anti_msg,then another thread
						//in the past (in wall clock time) has changed its state to extracted and executed it!!!!
					 	#if IPI==1

						#if DEBUG ==1
							if(event->frame==0){
								printf("event ANTI_MSG in past not EXECUTED\n");
								gdb_abort;
							}
						#endif

						#endif
						return_evt_to_main_loop();
					}//parentesi aggiunte da cristian insieme al debug==1
				}
				else if(curr_evt_state == ELIMINATED){//cristian:state is eliminated,remove it from calqueue
					do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);			//<<-ELIMINATED
				}
				
			}
			///* NOT VALID *///		
			else {
								
				///* MARK NON VALID NODE *///
				if( (curr_evt_state & ELIMINATED) == 0 ){ //cristian:state is "new_evt" or(exclusive) "extracted"
					//event is invalid but current_state is considered valid, set event_state in order to invalidate itself
					curr_evt_state = __sync_or_and_fetch(&(event->state),ELIMINATED);//cristian:new state is "eliminated" or(exclusive) "anti_msg"
				}

				///* DELETE ELIMINATED *///
				if( (curr_evt_state & EXTRACTED) == 0 ){//cristian:state is "new_evt" or(exclusive) "eliminated", but if it is 
					//cristian:new_evt it was changed to eliminated in the previous branch!!!so the event state is eliminated!!!!
					do_remove_removed_inside_lock_and_goto_next(event,node,lp_idx);					//<<-ELIMINATED
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
						do_remove_inside_lock_and_goto_next(event,node,lp_idx);
					}
					///* ANTI-MESSAGE IN THE PAST*///
					else{//cristian:anti_msg in past not handled,return it to simulation loop
						//cristian:if it is anti_msg,then another thread
						//in the past (in wall clock time) has changed its state to extracted and executed it!!!!
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
		}
	

get_next:
		// GET NEXT NODE PRECEDURE
		// From this point over the code is to retrieve the next event on the queue
		//cristian:retrieve next_node from calqueue that it is visible to simulation yet.
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

#if IPI==1

	//before exit from this function we must update current_lp,current_msg,current_node and safety

	current_lp=event->receiver_id;//update info on current_lp before execute swap_event
	if(current_lp!=lp_idx){
		gdb_abort;
	}
	event=swap_with_best_eventv3(event,from_new_evt_to_extracted_curr_evt,false);
	ts=event->timestamp;
    tb=event->tie_breaker;
    safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min) && (tb <= min_tb))) && !is_in_lp_unsafe_set(current_lp);
    current_msg =  event;//update current_msg
    current_node=container_of(&event,nbc_bucket_node,payload);//update current_node
    if(current_node->payload!=event){
    	printf("invalid current_node\n");
    	gdb_abort;
    }
else
	current_msg =  event;//(msg_t *) node->payload;
    current_node = node;
#endif
/*#if IPI==1
    if(lp_idx!=current_msg->receiver_id){
    	printf("invalid lp idx\n");
    	gdb_abort;
    }
    //current_lp = current_msg->receiver_id;
    lp_ptr = LPS[lp_idx];
    if(current_msg->state==EXTRACTED){
    	ts = current_msg->timestamp;
		tb = current_msg->tie_breaker;
		if(bound_ptr != lp_ptr->bound){
			printf("invalid bound_ptr\n");
			gdb_abort;
		}
		bound_ptr = lp_ptr->bound;
		lvt_ts = bound_ptr->timestamp; 
		lvt_tb = bound_ptr->tie_breaker;
		in_past = (ts < lvt_ts || (ts == lvt_ts && tb <= lvt_tb)); 
    	if(current_msg->frame==0 && in_past){
    		srand(time(NULL));   // Initialization, should only be called once.
    		int random_num=((int)rand())%20;// Returns a pseudo-random integer between 0 and RAND_MAX,[0,20-1] 
    		if(random_num<=2){
    		//skip/ignore this event and execute again fetch_internal_function
    			printf("event extracted in past not executed\n");
    			unlock(lp_idx);//unlock lp
    			fetch_internal();//retry fetch_internal
    		}
    	}
    	
    }
    //swap_with_best_evt(min,min_tb);
#endif*/


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
			//trovato il primo nodo valido, continuo localmente la ricerca
			while(current!=NULL && current->max_outgoing_ts < ts){
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

