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



__thread hpdcs_gc_status malloc_status =
{
	.free_nodes_lists 			= NULL,
	.to_free_nodes 				= NULL,
	.to_free_nodes_old 			= NULL,
	.block_size 				= sizeof(nbc_bucket_node),
	.offset_next 				= offsetof(nbc_bucket_node, next),
	.to_remove_nodes_count 		= 0LL
};

__thread nbc_bucket_node *to_free_tables_old = NULL;
__thread nbc_bucket_node *to_free_tables_new = NULL;

__thread unsigned long long mark;
__thread unsigned int to_remove_nodes_count = 0;

__thread list(msg_t) to_remove_local_evts = NULL;
__thread list(msg_t) freed_local_evts = NULL;


static unsigned int * volatile prune_array;
static unsigned int threads;

static nbc_bucket_node *g_tail;


/**
 * This function blocks the execution of the process.
 * Used for debug purposes.
 */
static void error(const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	printf("%s", buf);
	exit(1);
}

/**
 * This function computes the index of the destination bucket in the hashtable
 *
 * @author Romolo Marotta
 *
 * @param timestamp the value to be hashed
 * @param bucket_width the depth of a bucket
 *
 * @return the linear index of a given timestamp
 */
static inline unsigned long long hash(double timestamp, double bucket_width)
{
	double tmp1;
	double tmp2;
	double res_d = (timestamp / bucket_width);
	unsigned long long res =  (unsigned long long) res_d;


	if(res_d > 4294967295)
		error("Probable Overflow when computing the index: "
				"TS=%e,"
				"BW:%e, "
				"TS/BW:%e, "
				"2^32:%e\n",
				timestamp, bucket_width, res_d,  pow(2, 32));


	tmp1 = res * bucket_width;
	tmp2 = tmp1 + bucket_width;
    
	if(LESS(timestamp, tmp1))
		return --res;
	if(GEQ(timestamp, tmp2))
		return ++res;
	return res;

}

static inline bool is_marked_for_search(void *pointer, unsigned int research_flag)
{
	unsigned long long mask_value = (UNION_CAST(pointer, unsigned long long) & MASK_MRK);
	
	return 
		(/*research_flag == REMOVE_DEL &&*/ mask_value == DEL) 
		|| (research_flag == REMOVE_DEL_INV && (mask_value == INV) );
}
/**
 *  This function is an helper to allocate a node and filling its fields.
 *
 *  @author Romolo Marotta
 *
 *  @param payload is a pointer to the referred payload by the node
 *  @param timestamp the timestamp associated to the payload
 *
 *  @return the pointer to the allocated node
 *
 */
static nbc_bucket_node* node_malloc(void *payload, double timestamp, unsigned int tie_breaker)
{
	nbc_bucket_node* res;
	
	res = mm_node_malloc(&malloc_status);
	
	if (unlikely(is_marked(res) || res == NULL))
	{
		error("%lu - Not aligned Node or No memory\n", pthread_self());
		abort();
	}

	res->counter = tie_breaker;
	res->next = NULL;
	res->replica = NULL;
	res->payload = payload;
	res->epoch = 0;
	res->timestamp = timestamp;
	res->tag = -1;
	res->reserved = false;
#if DEBUG == 1 // TODO
	res->copy = 0;
	res->deleted = 0;
	res->executed = 0;	
#endif

	return res;
}

static void node_free(nbc_bucket_node *pointer)
{
	mm_node_free(&malloc_status, pointer);
}

/**
 * This function connect to a private structure marked
 * nodes in order to free them later, during a synchronisation point
 *
 * @author Romolo Marotta
 *
 * @param queue used to associate freed nodes to a queue
 * @param start the pointer to the first node in the disconnected sequence
 * @param number of node to connect to the to_be_free queue
 * @param timestamp   the timestamp of the last disconnected node
 *
 */
static inline void connect_to_be_freed_node_list(nbc_bucket_node *start, unsigned int counter)
{
	mm_node_trash(&malloc_status, get_unmarked(start), counter);
}

static inline void connect_to_be_freed_table_list(table *h)
{
	nbc_bucket_node *tmp = node_malloc(h, INFTY, 0);
	tmp->next = to_free_tables_new;
	to_free_tables_new = tmp;
}

/**
 * This function implements the search of a node that contains a given timestamp t. It finds two adjacent nodes,
 * left and right, such that: left.timestamp <= t and right.timestamp > t.
 *
 * Based on the code by Timothy L. Harris. For further information see:
 * Timothy L. Harris, "A Pragmatic Implementation of Non-Blocking Linked-Lists"
 * Proceedings of the 15th International Symposium on Distributed Computing, 2001
 *
 * @author Romolo Marotta
 *
 * @param queue the queue that contains the bucket
 * @param head the head of the list in which we have to perform the search
 * @param timestamp the value to be found
 * @param left_node a pointer to a pointer used to return the left node
 * @param left_node_next a pointer to a pointer used to return the next field of the left node 
 *
 */   

static void search(nbc_bucket_node *head, double timestamp, unsigned int tie_breaker,
						nbc_bucket_node **left_node, nbc_bucket_node **right_node, int flag)
{
	nbc_bucket_node *left, *right, *left_next, *tmp, *tmp_next, *tail;
	unsigned int counter;
	double tmp_timestamp;
	tail = g_tail;

	do
	{;
		/// Fetch the head and its next node
		tmp = head;
		tmp_next = tmp->next;
		left = tmp = head;
		left_next = tmp_next = tmp->next;
		assertf(head == NULL, "PANIC %s\n", "");
		assertf(tmp_next == NULL, "PANIC1 %s\n", "");
		counter = 0;
		do
		{
			// Check if the node is marked
			bool marked = is_marked_for_search(tmp_next, flag);

			// Find the first unmarked node that is <= timestamp
			if (!marked)
			{
				left = tmp;
				left_next = tmp_next;
				counter = 0;
			}
			// Take a count of the marked node between left node and current node (tmp)
			counter+=marked;

			// Retrieve timestamp and next field from the current node (tmp)
			tmp = get_unmarked(tmp_next);
			tmp_timestamp = tmp->timestamp;
			
			tmp_next = tmp->next;

			// Exit if tmp is a tail or its timestamp is > of the searched key
		} while (	tmp != tail &&
					(
						is_marked_for_search(tmp_next, flag) ||
						LESS(tmp_timestamp, timestamp)	||  
						(
							D_EQUAL(tmp_timestamp, timestamp) &&
							(
								tie_breaker == 0 || 
								(tie_breaker != 0 && tmp->counter <= tie_breaker)
							)
						)
					)
				);

		// Set right node and copy the mark of left node
		right = get_marked(tmp,get_mark(left_next));
		//right =  ((unsigned long long)tmp | (MASK_MRK &  (unsigned long long) left_next));

		//left node and right node have to be adjacent. If not try with CAS
		if (!is_marked_for_search(left_next, flag) && left_next != right)
		//if (left_next != right)
		{
			//LOG("TI ODIO %p %p %p %p %p\n", &(left->next), left, left_next, tail, &(tail->next));
			// if CAS succeeds connect the removed nodes to to_be_freed_list
			if (!BOOL_CAS(&(left->next), left_next, right))
					continue;
			connect_to_be_freed_node_list(left_next, counter);
		}
		
		// at this point they are adjacent
		*left_node = left;
		*right_node = right;
		
		return;
		
	} while (1);
}

/**
 * This function commits a value in the current field of a queue. It retries until the timestamp
 * associated with current is strictly less than the value that has to be committed
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param left_node the candidate node for being next current
 *
 */
static inline void nbc_flush_current(table* h, nbc_bucket_node* node)
{
	unsigned long long oldCur, oldIndex, oldEpoch;
	unsigned long long newIndex, newCur, tmpCur;
	
	// Retrieve the old index and compute the new one
	oldCur = h->current;
	oldEpoch = oldCur & MASK_EPOCH;
	oldIndex = oldCur >> 32;
	newIndex = ( unsigned long long ) hash(node->timestamp, h->bucket_width);
	newCur =  newIndex << 32;
	
	// Try to update the current if it need	
	if(
		newIndex >	oldIndex 
		|| is_marked(node->next)
		|| oldCur 	== 	(tmpCur =  VAL_CAS(
										&(h->current), 
										oldCur, 
										(newCur | (oldEpoch + 1))
									) 
						)
		)
	{
		return;
	}
						 
	//At this point someone else has update the current from the begin of this function
	do
	{
		oldCur = tmpCur;
		oldEpoch = oldCur & MASK_EPOCH;
		oldIndex = oldCur >> 32;
	}
	while (
		newIndex <	oldIndex 
		&& is_marked(node->next, VAL)
		&& oldCur 	!= 	(tmpCur = 	VAL_CAS(
										&(h->current), 
										oldCur, 
										(newCur | (oldEpoch + 1))
									) 
						)
		);
}

/**
 * This function insert a new event in the nonblocking queue.
 * The cost of this operation when succeeds should be O(1) as calendar queue
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param timestamp the timestamp of the event
 * @param payload the event to be enqueued
 *
 */
static bool insert_std(table* hashtable, nbc_bucket_node** new_node, int flag)
{
	nbc_bucket_node *left_node, *right_node, *bucket, *new_node_pointer;
	unsigned int index;

	unsigned int new_node_counter 	;
	double 		 new_node_timestamp ;

	new_node_pointer 	= (*new_node);
	new_node_counter 	= new_node_pointer->counter;
	new_node_timestamp 	= new_node_pointer->timestamp;

	index = hash(new_node_timestamp, hashtable->bucket_width) % hashtable->size;

	// node to be added in the hashtable
	bucket = hashtable->array + index;

	search(bucket, new_node_timestamp, new_node_counter, &left_node, &right_node, flag);

	if(!is_marked(right_node, MOV))
	{
		switch(flag)
		{
		case REMOVE_DEL_INV:

			new_node_pointer->next = right_node;
			// set tie_breaker
			new_node_pointer->counter = 1 + ( -D_EQUAL(new_node_timestamp, left_node->timestamp ) & left_node->counter );

			if (BOOL_CAS
					(
						&(left_node->next),
						right_node,
						new_node_pointer
					)
			)
			{
				#if LOG_ENQUEUE == 1
				LOG("ENQUEUE: %f %u - %u %u\n", new_node_pointer->timestamp, new_node_pointer->counter,	hash(new_node_timestamp, hashtable->bucket_width), index );
				#endif
				return true;
			}

			// reset tie breaker for the new search
			new_node_pointer->counter = 0;
			break;

		case REMOVE_DEL:

			// mark the to-be.inserted node as INV
			new_node_pointer->next = get_marked(right_node, INV);
			// node already exists
			if(D_EQUAL(new_node_timestamp, left_node->timestamp ) && left_node->counter == new_node_counter)
			{
				node_free(new_node_pointer);//<--------NEW
				//free(new_node_pointer);
				*new_node = left_node;
				return true;
			}
			// copy left node mark			
			new_node_pointer = get_marked(new_node_pointer,get_mark(right_node));
			//new_node_pointer =  (nbc_bucket_node*) (
			//					((unsigned long long) new_node_pointer) | 
			//					(MASK_MRK &  (unsigned long long) right_node)
			//					);

	//		if(right_node->timestamp == INFTY)
	//		printf("MOVING %f %u between left %f %u right  INFTY %u\n",
	//				new_node_timestamp, new_node_counter,
	//				left_node->timestamp, left_node->counter,
	//				 right_node->counter);
	//		else
	//			printf("MOVING %f %u between left %f %u right  %f %u\n",
	//					new_node_timestamp, new_node_counter,
	//					left_node->timestamp, left_node->counter,
	//					right_node->timestamp, right_node->counter);

			if (BOOL_CAS(
						&(left_node->next),
						right_node,
						new_node_pointer
					))
				//printf("insert_std: evt type %u\n", ((msg_t*)(new_node[0]->payload))->type);//da_cancellare
				return true;
			break;
		}
	}
	return false;

}

static void set_new_table(table* h, unsigned int threshold, double pub, unsigned int epb)
{
	nbc_bucket_node *tail = g_tail;
	int signed_counter = atomic_read( &h->counter );
	unsigned int counter = (unsigned int) ( (-(signed_counter >= 0)) & signed_counter);
	unsigned int i = 0;
	unsigned int size = h->size;
	unsigned int thp2;
	unsigned int new_size = 0;
	unsigned int res = 0;
	double pub_per_epb = pub*epb;
	table *new_h;
	nbc_bucket_node *array;
	
	thp2 = threshold *2;
	//size_thp2 = (unsigned int) ((thp2) / ( pub * epb ));
	//if(thp2 > size_thp2) thp2 = size_thp2;

//	if 		(size >= thp2/pub_per_epb && counter > 2   * (pub_per_epb*size))
//		new_size = 2   * size;
//	else if (size >  thp2/pub_per_epb && counter < 0.5 * (pub_per_epb*size))
//		new_size = 0.5 * size;
//	else if	(size == 1    && counter > thp2)
//		new_size = thp2/pub_per_epb;
//	else if (size == thp2/pub_per_epb && counter < threshold/pub_per_epb)
//		new_size = 1;

	//if 		(size >= thp2 && counter > 2   * (pub_per_epb*size))
	//	new_size = 2   * size;
	//else if (size >  thp2 && counter < 0.5 * (pub_per_epb*size))
	//	new_size = 0.5 * size;
	//else if	(size == 1    && counter > thp2)
	//	new_size = thp2;
	//else if (size == thp2 && counter < threshold)
	//	new_size = 1;

	//if 		(size >= thp2 && counter > 2*size)
	//	new_size = 2*size;
	//else if (size > thp2 && counter < 0.5*size)
	//	new_size =  0.5*size;
	//else if	(size == 1 && counter > thp2)
	//	new_size = thp2;
	//else if (size == thp2 && counter < threshold)
	//	new_size = 1;
	
	
	
	new_size += (-(size >= thp2 && counter > 2   * pub_per_epb * (size)) )	&(size <<1 );
	new_size += (-(size >  thp2 && counter < 0.5 * pub_per_epb * (size)) )	&(size >>1);
	new_size += (-(size == 1    && counter > thp2) 					   )	& thp2;
	new_size += (-(size == thp2 && counter < threshold)				   )	& 1;
	
	if(new_size != 0 && new_size <= MAXIMUM_SIZE)
	{
		res = posix_memalign((void**)&new_h, CACHE_LINE_SIZE, sizeof(table));
		if(res != 0)
			error("No enough memory to new table structure\n");

		//new_h = malloc(sizeof(table));
		//if(new_h == NULL)
		//	error("No enough memory to new table structure\n");

		new_h->bucket_width  = -1.0;
		new_h->size 		 = new_size;
		new_h->new_table 	 = NULL;
		new_h->counter.count = 0;
		new_h->current 		 = ((unsigned long long)-1) << 32;

		//array =  calloc(new_size, sizeof(nbc_bucket_node));
		array =  alloc_array_nodes(&malloc_status, new_size);
		if(array == NULL)
		{
			free(new_h);
			error("No enough memory to allocate new table array %u\n", new_size);
		}

		for (i = 0; i < new_size; i++)
		{
			array[i].next = tail;
			array[i].counter = 0;
			array[i].epoch = 0;
			array[i].replica = NULL;
		}

		new_h->array = array;

		if(!BOOL_CAS(&(h->new_table), NULL,	new_h))
		{
			free_array_nodes(&malloc_status, new_h->array);
			free(new_h);
			//free(new_h->array);
			//free(new_h);
		}
#if DEBUG == 1
		//else //DEBUG; decommentare
		//	printf("%u - CHANGE SIZE from %u to %u, items %u OLD_TABLE:%p NEW_TABLE:%p\n", TID, size, new_size, counter, h, new_h);
#endif
	}
}

static void block_table(table* h)
{
	unsigned int i=0;
	unsigned int size = h->size;
	nbc_bucket_node *array = h->array;
	nbc_bucket_node *bucket, *bucket_next;
	nbc_bucket_node *left_node, *right_node; 
	nbc_bucket_node *right_node_next, *left_node_next;
	nbc_bucket_node *tail = g_tail;
	double rand = 0.0;			// <----------------------------------------
	unsigned int start = 0;		// <----------------------------------------
	
	drand48_r(&seedT, &rand); // <----------------------------------------
	start = (unsigned int) rand * size;	// <----------------------------
	for(i = 0; i < size; i++)
	{
		bucket = array + ((i + start) % size);	// <---------------------
		//bucket = array + ((i + (TID)) % size);
		//Try to ark the head as MOV
		do
		{
			bucket_next = bucket->next;
		}
		while( !is_marked(bucket_next, MOV) &&
				!BOOL_CAS(&(bucket->next), bucket_next,	get_marked(bucket_next, MOV)) 
		);
		//Try to mark the first VALid node as MOV
		do
		{
			search(bucket, -1.0, 0, &left_node, &left_node_next, REMOVE_DEL_INV);
			right_node = get_unmarked(left_node_next);
			right_node_next = right_node->next;	
		}
		while(
				right_node != tail &&
				(
					is_marked(right_node_next, DEL) ||
					(
						is_marked(right_node_next, VAL) 
						&& !BOOL_CAS(&(right_node->next), right_node_next, get_marked(right_node_next, MOV))
					)
				)
		);
	}
}

static double compute_mean_separation_time(table* h,
		unsigned int new_size, unsigned int threashold, unsigned int elem_per_bucket)
{
	nbc_bucket_node *tail = g_tail;

	unsigned int i = 0, index;

	table *new_h = h->new_table;
	nbc_bucket_node *array = h->array;
	double old_bw = h->bucket_width;
	unsigned int size = h->size;
	double new_bw = new_h->bucket_width;
	
	unsigned int sample_size;
	double average = 0.0;
	double newaverage = 0.0;
	double tmp_timestamp;
	unsigned int counter = 0;
	
	double min_next_round = INFTY;
	double lower_bound, upper_bound;
    
    nbc_bucket_node *tmp, *tmp_next;
	
	index = (unsigned int)(h->current >> 32);
	
	if(new_bw >= 0)
		return new_bw;
	
	if(new_size <= threashold*2)
		return 1.0;
	
	sample_size = (new_size <= 5) ? (new_size/2) : (5 + (unsigned int)((double)new_size / 10));
	sample_size = (sample_size > SAMPLE_SIZE) ? SAMPLE_SIZE : sample_size;
    
	double sample_array[SAMPLE_SIZE+1]; //<--DA SISTEMARE STANDARD C90
    
    //read nodes until the total samples is reached or until someone else do it
	while(counter != sample_size && new_h->bucket_width == -1.0)
	{   
		for (i = 0; i < size; i++)
		{
			tmp = array + (index + i) % size; 	//get the head of the bucket
			tmp = get_unmarked(tmp->next);		//pointer to first node
			
			lower_bound = (index + i) * old_bw;
			upper_bound = (index + i + 1) * old_bw;
		
			while( tmp != tail && counter < sample_size )
			{
				tmp_timestamp = tmp->timestamp;
				tmp_next = tmp->next;
				//I will consider ognly valid nodes (VAL or MOV) In realtà se becco nodi MOV posso uscire!
				if(!is_marked(tmp_next, DEL) && !is_marked(tmp_next, INV))
				{
					if( //belong to the current bucket
						LESS(tmp_timestamp, upper_bound) &&	GEQ(tmp_timestamp, lower_bound) &&
						!D_EQUAL(tmp_timestamp, sample_array[counter])
					)
					{
						sample_array[++counter] = tmp_timestamp;
					}
					else if(GEQ(tmp_timestamp, upper_bound) && LESS(tmp_timestamp, min_next_round))
					{
							min_next_round = tmp_timestamp;
							break;
					}
				}
				tmp = get_unmarked(tmp_next);
			}
		}
		//if the calendar has no more elements I will go out
		if(min_next_round == INFTY)
			break;
		//otherwise I will restart from the next good bucket
		index = hash(min_next_round, old_bw);
		min_next_round = INFTY;
	}


	
	if( counter < sample_size)
		sample_size = counter;
    
	for(i = 2; i<=sample_size;i++)
		average += sample_array[i] - sample_array[i - 1];
    
		// Get the average
	average = average / (double)(sample_size - 1);
    
	int j=0;
	// Recalculate ignoring large separations
	for (i = 2; i <= sample_size; i++) {
		if ((sample_array[i] - sample_array[i - 1]) < (average * 2.0))
		{
			newaverage += (sample_array[i] - sample_array[i - 1]);
			j++;
		}
	}
    
	// Compute new width
	newaverage = (newaverage / j) * elem_per_bucket;	/* this is the new width */
	if(newaverage <= 0.0)
		newaverage = 1.0;
	//if(newaverage <  pow(10,-4))
	//	newaverage = pow(10,-3);
	if(isnan(newaverage))
		newaverage = 1.0;	
    
	return newaverage;
}

static void migrate_node(nbc_bucket_node *right_node,	table *new_h)
{
	nbc_bucket_node *replica, *right_replica_field, *right_node_next;
	
	//Create a new node inserted in the new table as as INValid
	replica = node_malloc(right_node->payload, right_node->timestamp,  right_node->counter);
	replica->reserved = right_node->reserved;
	replica->tag = right_node->tag;
#if DEBUG == 1
	replica->copy = right_node->copy + 1;
	replica->deleted = right_node->deleted;
	replica->executed = right_node->executed;
#endif
	         
    do
	{ 
		right_replica_field = right_node->replica;
	}        
	while(right_replica_field == NULL && !insert_std(new_h, &replica, REMOVE_DEL));
	
	if( right_replica_field == NULL && 
			BOOL_CAS(
				&(right_node->replica),
				NULL,
				replica
				)
		)
	{
		ATOMIC_INC(&(new_h->counter));
    }
             
	right_replica_field = right_node->replica;
            
	do
	{
		right_node_next = right_replica_field->next;
	}while( 
		is_marked(right_node_next, INV) && 
		!BOOL_CAS(	&(right_replica_field->next),
					right_node_next,
					get_unmarked(right_node_next)
				)
		);

	nbc_flush_current(new_h, right_replica_field);
	
	
	right_node_next = FETCH_AND_AND(&(right_node->next), MASK_DEL);
	//right_node_next = __sync_and_and_fetch(&(right_node->next), MASK_DEL);
	//LOG("VALUE after fetch&and %p\n", right_node_next);
	
	//do{
	//	right_node_next = right_node->next;
	//}while( 
	//	!is_marked(right_node_next, DEL) && 
	//	!BOOL_CAS(	&(right_node->next),
	//				right_node_next,
	//				get_marked(get_unmarked(right_node_next), DEL)
	//			) 
	//	) ;
	
}

static table* read_table(nb_calqueue *queue)
{
	table *h = queue->hashtable		;
#if ENABLE_EXPANSION == 0
	return h;
#endif
	nbc_bucket_node *tail = g_tail	;
	unsigned int i, size = h->size	;

	table 			*new_h 			;
	double 			 new_bw 		;
	double 			 newaverage		;
	double 			 pub_per_epb	;
	nbc_bucket_node *bucket, *array	;
	nbc_bucket_node *right_node, *left_node, *right_node_next, *node;
	
	int counter = atomic_read(&h->counter);
	
	//printf("SIZE H %d\n", h->counter.count);
	
	pub_per_epb = queue->pub_per_epb;
	if( 
		(	
			counter < pub_per_epb * 0.5 * size ||
			counter > pub_per_epb * 2   * size
		)
		//(counter < 0.5*size || counter > 2*size)
		&& (h->new_table == NULL)
		)
		set_new_table(h, queue->threshold, queue->perc_used_bucket, queue->elem_per_bucket);

	if(h->new_table != NULL)
	{
		//printf("%u - MOVING BUCKETS\n", tid);
		new_h 			= h->new_table;
		array 			= h->array;
		new_bw 			= new_h->bucket_width;

		if(new_bw < 0)
		{
			block_table(h);
			newaverage = compute_mean_separation_time(h, new_h->size, queue->threshold, queue->elem_per_bucket);
			if
			(
				BOOL_CAS(
						UNION_CAST(&(new_h->bucket_width), unsigned long long *),
						UNION_CAST(new_bw,unsigned long long),
						UNION_CAST(newaverage, unsigned long long)
					)
			)
				LOG("COMPUTE BW -  OLD:%.20f NEW:%.20f %u\n", new_bw, newaverage, new_h->size);
		}

		//First try: try to migrate the nodes, if a marked node is found, continue to the next bucket
		double rand;			// <----------------------------------------
		unsigned int start;		// <----------------------------------------
		
		drand48_r(&seedT, &rand); // <----------------------------------------
		start = (unsigned int) rand * size;	// <----------------------------
		for(i = 0; i < size; i++)
		{
			bucket = array + ((i + start) % size);	// <---------------------
		//for(i=0; i < size; i++)
		//{
			//bucket = array + ((i + (TID)) % size);
			node = get_unmarked(bucket->next);		//node = left_node??
			do
			{
				if(node == tail)
					break;
				//Try to mark the top node
				search(node, -1.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
			
				right_node = get_unmarked(right_node);
				right_node_next = right_node->next;
        
				if( right_node == tail ||
					is_marked(right_node_next) || 
						!BOOL_CAS(
								&(right_node->next),
								right_node_next,
								get_marked(right_node_next, MOV)
							)								
				)
					break;
				
				migrate_node(node, new_h);
				node = right_node;
				
			}while(true);
		}
	
		//Second try: try to migrate the nodes and continue until each bucket is empty
		drand48_r(&seedT, &rand); // <----------------------------------------
		start = (unsigned int) rand + size;	// <----------------------------
		for(i = 0; i < size; i++)
		{
			bucket = array + ((i + start) % size);	// <---------------------
		//for(i=0; i < size; i++)
		//{
		//	bucket = array + ((i + (TID)) % size);
			node = get_unmarked(bucket->next);		//node = left_node??
			do
			{
				if(node == tail)
					break;
				//Try to mark the top node
					
				do
				{
					search(node, -1.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
					right_node = get_unmarked(right_node);
					right_node_next = right_node->next;
				}
				while(
						right_node != tail &&
						(
							is_marked(right_node_next, DEL) ||
							(
								is_marked(right_node_next, VAL) 
								&& !BOOL_CAS(&(right_node->next), right_node_next, get_marked(right_node_next, MOV))
							)
						)
				);
			
				if(is_marked(node->next, MOV))
				{
					migrate_node(node, new_h);
				}
				node = right_node;
				
			}while(true);
	
			search(bucket, -1.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
			assertf(get_unmarked(right_node) != tail, "Fail in line 972 %p %p %p %p %p %p\n",
			 bucket,
			  left_node,
			   right_node, 
			   ((nbc_bucket_node*)get_unmarked(right_node))->next, 
			   ((nbc_bucket_node*)get_unmarked(right_node))->replica, 
			 //  ((nbc_bucket_node*)get_unmarked(right_node))->replica->next, 
			   tail); 
	
		}

		//Try to replace the old table with the new one
		if( BOOL_CAS(&(queue->hashtable), h, new_h) )
		{
			connect_to_be_freed_table_list(h);
		}

		h = new_h;
	}
	return h;
}

/**
 * This function create an instance of a non-blocking calendar queue.
 *
 * @author Romolo Marotta
 *
 * @param queue_size is the inital size of the new queue
 *
 * @return a pointer a new queue
 */
nb_calqueue* nb_calqueue_init(unsigned int threshold, double perc_used_bucket, unsigned int elem_per_bucket)
{
	unsigned int i = 0;
	//unsigned int new_threshold = 1;		//<-----NEW
	unsigned int res_mem_posix = 0;		//<-----NEW

	threads = threshold;
	prune_array = calloc(threshold*threshold, sizeof(unsigned int));

	nb_calqueue* res = calloc(1, sizeof(nb_calqueue));
	if(res == NULL)
		error("No enough memory to allocate queue\n");
		
	//while(new_threshold <= threshold)	//<-----NEW
	//	new_threshold <<=1;				//<-----NEW

	res->threshold = threshold;
	res->perc_used_bucket = perc_used_bucket;
	res->elem_per_bucket = elem_per_bucket;
	res->pub_per_epb = perc_used_bucket * elem_per_bucket;

	//res->hashtable = malloc(sizeof(table));
	//if(res->hashtable == NULL)
	res_mem_posix = posix_memalign((void**)&res->hashtable, CACHE_LINE_SIZE, sizeof(table));//<-----NEW
	if(res_mem_posix != 0)				//<-----NEW
	{
		free(res);
		error("No enough memory to allocate queue\n");
	}
	res->hashtable->bucket_width = 1.0;
	res->hashtable->new_table = NULL;
	res->hashtable->size = MINIMUM_SIZE;

	//res->hashtable->array = calloc(MINIMUM_SIZE, sizeof(nbc_bucket_node) );
	res->hashtable->array =  alloc_array_nodes(&malloc_status, MINIMUM_SIZE);//<-----NEW
	if(res->hashtable->array == NULL)
	{
		free(res->hashtable);
		free(res);
		error("No enough memory to allocate queue\n");
	}

	g_tail = node_malloc(NULL, INFTY, 0);
	g_tail->next = NULL;

	for (i = 0; i < MINIMUM_SIZE; i++)
	{
		res->hashtable->array[i].next = g_tail;
		res->hashtable->array[i].timestamp = i * 1.0;
		res->hashtable->array[i].counter = 0;
	}

	res->hashtable->current = 0;

	return res;
}

/**
 * This function implements the enqueue interface of the non-blocking queue.
 * Should cost O(1) when succeeds
 *
 * @author Romolo Marotta
 *
 * @param queue
 * @param timestamp the key associated with the value
 * @param payload the event to be enqueued
 *
 * @return true if the event is inserted in the hashtable, else false
 */
void nbc_enqueue(nb_calqueue* queue, double timestamp, void* payload, unsigned int tag)
{
	// TODO
	nbc_bucket_node *new_node = node_malloc(payload, timestamp, 0);
	table *h, *old_h = NULL;	
	
	if(new_node->payload != NULL)//DEBUG/TODO
		((msg_t*)new_node->payload)->node=0xBADC0DE;
	
	new_node->tag = tag;

	do
	{
		if(old_h != (h = read_table(queue)))
		{
			old_h = h;
			new_node->epoch = 0;//(h->current & MASK_EPOCH);
		}
	} while(!insert_std(h, &new_node, REMOVE_DEL_INV));

	//nbc_flush_current(h, new_node);
	((msg_t*)new_node->payload)->tie_breaker = new_node->counter; //TODO//CONTROLLARE//PADS
	
	ATOMIC_INC(&(h->counter));
}

static inline bool CAS_for_mark( nbc_bucket_node* right_node, nbc_bucket_node* right_node_next)
{
	return BOOL_CAS(
			&(right_node->next),
			get_unmarked(right_node_next),
			get_marked(right_node_next, DEL)
			);

}

static inline bool CAS_for_increase_cur(table* h, unsigned long long current, unsigned long long newCur)
{
	return BOOL_CAS(
			&(h->current),
			current,
			newCur				);
}

/**
 * This function frees any node in the hashtable with a timestamp strictly less than a given threshold,
 * assuming that any thread does not hold any pointer related to any nodes
 * with timestamp lower than the threshold.
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param timestamp the threshold such that any node with timestamp strictly less than it is removed and freed
 *
 */
 
 
void nbc_prune(void)
{
#if ENABLE_PRUNE == 0
	return;
#endif
	
	nbc_bucket_node  *tmp, *tmp_next;
	unsigned int counter;
	
	
	if(!mm_safe(prune_array, threads, TID))
		return;
		
	
	while(to_free_tables_old != NULL)
	{
		nbc_bucket_node *my_tmp = to_free_tables_old;
		to_free_tables_old = to_free_tables_old->next;
    
		table *h = (table*) my_tmp->payload;
		free_array_nodes(&malloc_status, h->array); //<-------NEW
		free(h);
		node_free(my_tmp); //<-------NEW
	}
	
	do 														//<-----NEW
    {	                                                    //<-----NEW
		tmp = mm_node_collect(&malloc_status, &counter);    //<-----NEW
		while(tmp != NULL && counter-- != 0)                //<-----NEW
		{                                                   //<-----NEW
			tmp_next = tmp->next;                           //<-----NEW
			node_free(tmp);                                 //<-----NEW
			tmp =  get_unmarked(tmp_next);                  //<-----NEW
		}                                                   //<-----NEW
	}                                                       //<-----NEW
    while(tmp != NULL);										//<-----NEW

	to_free_tables_old = to_free_tables_new;
	to_free_tables_new = NULL;
	
	mm_new_era(&malloc_status, prune_array, threads, TID);
	
}


nbc_bucket_node* getMin(nb_calqueue *queue, table ** hres){
		nbc_bucket_node *min, *min_next, 
					*left_node, *left_node_next, 
					*tail, *array;
	table * h;
	
	unsigned long long current;
	unsigned long long index;
	unsigned long long epoch;
	
	unsigned int size;
	unsigned int counter;
	double bucket_width;

	tail = g_tail;
	
	do
	{
		counter = 0;
		h = read_table(queue);
		current = h->current;
		size = h->size;
		array = h->array;
		bucket_width = h->bucket_width;

		index = current >> 32;
		epoch = current & MASK_EPOCH;

		assertf(
				index+1 > MASK_EPOCH, 
				"\nOVERFLOW INDEX:%llu  BW:%.10f SIZE:%u TAIL:%p TABLE:%p NUM_ELEM:%u\n",
				index, bucket_width, size, tail, h, atomic_read(&h->counter)
			);
		min = array + (index++ % size);

		left_node = min_next = min->next;
		
		if(is_marked(min_next, MOV))
			continue;
		
		while(left_node->epoch <= epoch)
		{	
			left_node_next = left_node->next;
			if(!is_marked(left_node_next))
			{
				if(left_node->timestamp < index*bucket_width)
				{
#if DEBUG == 1
					if(left_node == g_tail){
						printf("[%u] ERROR: getNext is returning a tail\n", tid);
					}
#endif
					*hres = h;
					return left_node;
				}
				else
				{
					if(left_node == tail && size == 1 && !is_marked(min->next, MOV))
					{
						#if LOG_DEQUEUE == 1
						LOG("DEQUEUE: NULL 0 - %llu %llu\n", index, index % size);
						#endif
						//printf("CODA VUOTA!!!!\n");
						return NULL;
					}
					if(counter > 0 && BOOL_CAS(&(min->next), min_next, left_node))
					{
						connect_to_be_freed_node_list(min_next, counter);
					}
					BOOL_CAS(&(h->current), current, ((index << 32) | epoch) );
					break;	
				}
				
			}
			
			if(is_marked(left_node_next, MOV))
			{
				break;
			}
			left_node = get_unmarked(left_node_next);
			counter++;
		}

	}while(1);
	
	return NULL;
}


bool delete(nb_calqueue *queue, nbc_bucket_node* node){
	nbc_bucket_node *node_next, *tmp;
	table *h = read_table(queue);
    node_next = FETCH_AND_OR(&node->next, DEL);
    tmp = node;
    while(tmp->replica != NULL || is_marked(node_next, MOV))
    {
        h = read_table(queue);
        tmp = tmp->replica;
        node_next = FETCH_AND_OR(&tmp->next, DEL);
       
    }
    if(is_marked(node_next, VAL))
		ATOMIC_DEC(&(h->counter));
    //ATOMIC_DEC(&(h->counter));
    
#if DEBUG == 1
	tmp->deleted = tmp->deleted + 1;
#endif
    return is_marked(node_next, VAL);
}


nbc_bucket_node* unmarked(void *pointer){ //da cancellare
	return (nbc_bucket_node *)((unsigned long long) pointer & MASK_PTR) ;
}


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



#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event),event, event->node);

//
//unsigned int fetch_internal(){
//	table *h;
//	nbc_bucket_node * node, *node_next, *min_node;
//	simtime_t ts, min = INFTY, lvt_ts;
//	unsigned int lp_idx, bucket, size, tail_counter = 0, lvt_tb;
//	double bucket_width;
//	LP_state *lp_ptr;
//	msg_t *event, *local_next_evt, *tmp_node, * bound_ptr;
//	
//	current_msg = NULL; //DEBUG
//	
//	// Get the minimum node from the calendar queue
//    if((node = min_node = getMin(nbcalqueue, &h)) == NULL)
//		return 0;
//	
//	safe = false;
//	clear_lp_unsafe_set; //Set S ← NULL
//	//commit_horizon_ts = min = node->timestamp; //time min ← evt.ts
//    //commit_horizon_tb = node->counter;
//    
//	//h = read_table(nbcalqueue);						//
//	bucket_width = h->bucket_width;					//
//	bucket = hash(node->timestamp, bucket_width);	//
//	size = h->size;								//
//    
//    // Explore the queue looking for the next event
//    // which is 'processable', namely one of the following:
//    // 1. Valid executable event (in the future wrt lp's lvt)
//    // 2. Valid straggler executable event (in the past wrt lp's lvt)
//    // 3. Invalid event which must be rolled back
//    // The loop will terminate if one of the above conditions is met OR
//    // if the queue has no more event to explore.
//    while(node != NULL){
//		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
//		ts = node->timestamp;				// Timestamp of the event under exploration
//		event = (msg_t *)node->payload;		// Event under exploration
//#if DEBUG==1
//		if(ts != event->timestamp) printf(RED("Ma vallallero N.TS:%f E.TS:%f\n"), ts, event->timestamp);
//#endif		
//		//lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
//		//bound_ptr = lp_ptr->bound;
//		//lvt_ts = bound_ptr->timestamp; 
//		//lvt_tb = bound_ptr->tie_breaker; 
//		
//
//		//TODO verificare
//		// Set the safety of the current event under exploration
//		
//		///* VALID *///
//		// The event under exploration is considered valid if it does belong to
//		// current and correct timeline. The correct timeline is the one will be
//		// observable at the end of a correct simulation process.
//		if(is_valid(event)){
//			if(min == INFTY){
//				commit_horizon_ts = min = node->timestamp; //time min ← evt.ts
//				commit_horizon_tb = node->counter;
//			}
//			safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && ts == min)) && !is_in_lp_unsafe_set(lp_idx);
//		
//			lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
//valid:
//			bound_ptr = lp_ptr->bound;
//			lvt_ts = bound_ptr->timestamp; 
//			lvt_tb = bound_ptr->tie_breaker; 
//		
//			///* VALID AND EXECUTED *///
//			if((event->state == EXTRACTED) && (ts < lvt_ts || (ts == lvt_ts && node->counter <= lvt_tb) )){
//				///* VALID AND EXECUTED AND SAFE*///
//				if(safe) {
//					if(node == min_node)//DEBUG
//						delete(nbcalqueue, node);
//				}
//			}
//			///* VALID AND NOT EXECUTED AND LOCK TAKEN *///
//			// The event under exploration is valid, not extracte yet and appears to be in the
//			// future of the LP, therefore we need to check whether to execute it or the one
//			// within the LP's local queue.
//			else if(tryLock(lp_idx)) {
//				//If the bound is changed, check again
//				if(bound_ptr != lp_ptr->bound){
//					unlock(lp_idx);
//					goto valid;
//				}
//#if DEBUG==1
//				if(event->state == EXTRACTED && node->epoch == lp_ptr->epoch){
//					printf("ERRORE: stai rieseguendo un evento con epoca coerente! Event_pointer:%p \n\Destinatio:%u Timestamp:%f Executed:%u\n\n",node,lp_idx,node->timestamp,node->executed);
//					gdb_abort;
//				}
//#endif
//				
//				///("GET_NEXT_EXECUTED_AND_VALID\n");
//
//				// TODO: perché non spostare questa operazione nel thread loop?
//				local_next_evt = list_next(lp_ptr->bound);
//				while(local_next_evt != NULL && !is_valid(local_next_evt)) {
//					//tmp_node = list_next(local_next_evt);
//					list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
//					list_place_after_given_node_by_content(TID, to_remove_local_evts, 
//														local_next_evt, ((rootsim_list *)to_remove_local_evts)->head->data);
//					local_next_evt = list_next(lp_ptr->bound);
//					// TODO: valutare cancellazione da coda globale
//				}
//
//				if( local_next_evt != NULL && 
//					(
//						local_next_evt->timestamp < ts || 
//						(local_next_evt->timestamp == ts && local_next_evt->tie_breaker < node->counter)
//					)
//				  ) {
//					printf("Sto pescando un evento dalla coda locale %u con BOUND.TS:%f BOUND.TB:%u. \nIn mano ho \n\t", lp_idx,lvt_ts,lvt_tb );
//					print_event(event);
//					printf("e lo sto sostituendo con \n\t");
//					print_event(local_next_evt);
//					printf("Bound \n\t");
//					print_event(lp_ptr->bound);
//					printf("\n\t");
//					
//					event = local_next_evt;
//					node  = local_next_evt->node;
//					
//					if(0xbadc0de == node){//DEBUG
//						printf(RED("1 - BADCODE REACHED state:%u\n"), event->state);
//					}
//					printf("GET_NEXT_AND_VALID_SUCCESSFULL\n");
//				}
//			    //Marco l'evento come estratto se ancora non lo è
//			    ///* VALID AND NOT EXECUTED AND LOCK TAKEN AND NOT EXTRACTED YET *///
//				if(event->state == 0x0) { //IF evt.state = NULL
//					///* VALID AND NOT EXECUTED AND LOCK TAKEN AND NOT EXTRACTED YET BUT CONCURRENTLY ANNILATED *///
//					/* Segnala che l’evento è stato estratto almeno una volta */
//					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){ //IF OR_AND_FETCH(&evt.state, ESTRATTO)=TO_REMOVE 
//						delete(nbcalqueue, node);
//						printf("DELETE FROM QUEUE BY CONCURRENT ELIMINATION\n");/* DELETE_FROM_QUEUE */
//						unlock(lp_idx);
//					}
//					else {
//						break;
//					}
//				}
//				///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
//				else {
//					//if(event->timestamp == bound_ptr->timestamp && event->tie_breaker != bound_ptr->tie_breaker)
//					//{
//					//	printf("B - SAME TS AS BOUND  : LID:%d LP.LVT:%f-%d-%d CURR_LVT:%f-%d-%d EX_FR:%d\n",
//					//	 lp_idx, 
//					//	 bound_ptr->timestamp, bound_ptr->tie_breaker, is_valid(bound_ptr),
//					//	 event->timestamp, event->tie_breaker, is_valid(event), 
//					//	 LPS[lp_idx]->num_executed_frames);
//					//}
//					if(
//						event->timestamp < bound_ptr->timestamp ||
//						(
//							event->timestamp == bound_ptr->timestamp 
//							&& event->tie_breaker <= bound_ptr->tie_breaker
//						)
//					){
//						unlock(lp_idx);
//						add_lp_unsafe_set(lp_idx);
//					}
//					else
//						break;
//				}
//			}
//			///* VALID AND NOT EXECUTED AND LOCK NOT TAKEN *///
//			else {
//				add_lp_unsafe_set(lp_idx);
//			}
//		}
//		///* NOT VALID *///		
//		else {
//			///* NOT VALID AND ELIMINATED*///
//			if(event->state != ANTI_EVENT && 
//				( event->state == ELIMINATED || __sync_or_and_fetch(&event->state, ELIMINATED)==ELIMINATED)){
//				delete(nbcalqueue, node);
//				printf("DELETE FROM QUEUE BY ELIMINATION\n");/* DELETE_FROM_QUEUE */
//			}
//			///* NOT VALID AND TO BE ROLLBACKED AND LOCK TAKEN *///
//			else if(tryLock(lp_idx)) {
//				delete(nbcalqueue, node);
//				printf("DELETE FROM QUEUE BY ROLLBACK\n");/* DELETE_FROM_QUEUE */
//				break;
//			}
//			///* NOT VALID AND TO BE ROLLBACKED AND LOCK NOT TAKEN *///
//			else {
//				add_lp_unsafe_set(lp_idx);
//			} 
//		}
//
//		// GET NEXT NODE PRECEDURE
//		// From this point over the code is to retrieve the next event on the queue
//		do {
//			node_next = node->next;
//			if(is_marked(node_next, MOV) || node->replica != NULL) {
//					printf("FETCH DONE 4 TABLE:%p NODE:%p NODE_NEXT:%p NODE_REPLICA:%p TAIL:%p counter:%u\n", h, node,node->next, node->replica, g_tail, node->counter);
//					return 0;
//				}
//			
//			do {
//				node = get_unmarked(node_next);
//				node_next = node->next;
//			} while(
//				(is_marked(node_next, DEL) || is_marked(node_next, INV) ) ||
//				(node->timestamp < bucket * bucket_width )
//			);
//				
//			if( (bucket)*bucket_width <= node->timestamp && node->timestamp < (bucket+1)*bucket_width){
//				if(is_marked(node_next, MOV) || node->replica != NULL){
//    					printf("FETCH DONE 3\n");
//						return 0;
//					}
//				break;
//			}
//			else {
//				if(node == g_tail){
//					if(++tail_counter >= size){
//    					//printf("FETCH DONE 2: SIZE:%u\n", size);
//						return 0;
//					}
//				}
//				else {
//					tail_counter = 0;
//				}
//				node = h->array + (++bucket % size);
//			}
//		} while(1);
//    }
// 
// 	if(node == NULL)
//        return 0;
// 
//    //node->reserved = true;
//    // Set the global variables with the selected event to be processed
//    // in the thread main loop.
//    current_msg =  event;//(msg_t *) node->payload;
//    if(current_msg->node == 0xbadc0de){
//		current_msg->node = node;
//		current_msg->tie_breaker = node->counter;
//		//printf("Sto pescando un evento già pescato\n");
//	}
//#if DEBUG==1
//    if(event->state == EXTRACTED) node->executed++;
//    if(node->executed>2){
//		printf("Lo stesso evento è stato eseguito %u volte.\nEvent_pointer:%p Timestamp:%f\n\n",node->executed,node,node->timestamp);
//	}
//#endif
//    
//    return 1;
//}
//


unsigned int fetch_internal(){
	table *h;
	nbc_bucket_node * node, *node_next, *min_node;
	simtime_t ts, min = INFTY, lvt_ts;
	unsigned int lp_idx, bucket, size, tail_counter = 0, lvt_tb;
	double bucket_width;
	LP_state *lp_ptr;
	msg_t *event, *local_next_evt, *tmp_node, * bound_ptr;
	
	current_msg = NULL; //DEBUG
	unsigned int c = 0;
	
	// Get the minimum node from the calendar queue
    if((node = min_node = getMin(nbcalqueue, &h)) == NULL)
		return 0;
	
	safe = false;
	clear_lp_unsafe_set; //Set S ← NULL
	
	//h = read_table(nbcalqueue);						//
	bucket_width = h->bucket_width;					//
	bucket = hash(node->timestamp, bucket_width);	//
	size = h->size;								//
    
    // Explore the queue looking for the next event
    // which is 'processable', namely one of the following:
    // 1. Valid executable event (in the future wrt lp's lvt)
    // 2. Valid straggler executable event (in the past wrt lp's lvt)
    // 3. Invalid event which must be rolled back
    // The loop will terminate if one of the above conditions is met OR
    // if the queue has no more event to explore.
    while(node != NULL){	
		//TODO verificare
		if(++c%1000==0){
			printf("Eventi scorsi in fetch %u\n",c);
		}
		// Set the safety of the current event under exploration
		
		event = (msg_t *)node->payload;		// Event under exploration
		lp_idx = node->tag;					// Index of the LP relative to the event under exploration
		ts = node->timestamp;				// Timestamp of the event under exploration
		lp_ptr = LPS[lp_idx];				// State of the LP relative of the event under exploration
				

		///* VALID *///
		// The event under exploration is considered valid if it does belong to
		// current and correct timeline. The correct timeline is the one will be
		// observable at the end of a correct simulation process.

	if(tryLock(lp_idx)) {
		if(is_valid(event)) {
			if(min == INFTY){
				commit_horizon_ts = min = node->timestamp; //time min ← evt.ts
				commit_horizon_tb = node->counter;	
			}
			
			safe = ((ts < (min + LOOKAHEAD)) || (LOOKAHEAD == 0 && (ts == min))) && 
				!is_in_lp_unsafe_set(lp_idx);
		

			///* VALID AND NOT EXECUTED AND LOCK TAKEN *///
			// The event under exploration is valid, not extracte yet and appears to be in the
			// future of the LP, therefore we need to check whether to execute it or the one
			// within the LP's local queue.
			///* VALID AND NOT EXECUTED AND LOCK TAKEN *///
			

new_bound:
				bound_ptr = lp_ptr->bound;
				lvt_ts = bound_ptr->timestamp; 
				lvt_tb = bound_ptr->tie_breaker; 

				///* VALID AND EXECUTED AND MIN *///
				if((node == min_node)){//DEBUG
					if((event->state == EXTRACTED)&&
						(event->timestamp < bound_ptr->timestamp ||
						(
							event->timestamp == bound_ptr->timestamp 
							&& event->tie_breaker <= bound_ptr->tie_breaker
						))
					){
						delete(nbcalqueue, node);
						check_OnGVT(lp_idx);
						//event->node = 0x5AFE;
						unlock(lp_idx);
						return 0;
					}
				}
			
				///("GET_NEXT_EXECUTED_AND_VALID\n");
				local_next_evt = list_next(lp_ptr->bound);
				while(local_next_evt != NULL && !is_valid(local_next_evt)) {
					list_extract_given_node(lp_ptr->lid, lp_ptr->queue_in, local_next_evt);
					//list_place_after_given_node_by_content(TID, to_remove_local_evts, 
					//									local_next_evt, ((rootsim_list *)to_remove_local_evts)->head->data);
					local_next_evt->state = ANTI_EVENT; //DEBUG									
					local_next_evt = list_next(lp_ptr->bound);
				}
				
				if( local_next_evt != NULL && 
					(
						local_next_evt->timestamp < ts || 
						(local_next_evt->timestamp == ts && local_next_evt->tie_breaker < node->counter)
					)
				  ) {
					event = local_next_evt;
					node  = local_next_evt->node;
					ts = event->timestamp;
					//printf("GET_NEXT_AND_VALID: "); print_event(event);
					if(0xbadc0de == node){ //DEBUG
						printf(RED("1 - BADCODE REACHED state:%u\n"), event->state);
						gdb_abort;
					}
				
				}
			    //Marco l'evento come estratto se ancora non lo è
			    ///* VALID AND NOT EXECUTED AND LOCK TAKEN AND NOT EXTRACTED YET *///
				if(event->state == 0x0) { //IF evt.state = NULL
					///* VALID AND NOT EXECUTED AND LOCK TAKEN AND NOT EXTRACTED YET BUT CONCURRENTLY ANNILATED *///
					/* Segnala che l’evento è stato estratto almeno una volta */
					if(__sync_or_and_fetch(&(event->state),EXTRACTED) != EXTRACTED){ //IF OR_AND_FETCH(&evt.state, ESTRATTO)=TO_REMOVE 
						printf("DELETE FROM QUEUE BY CONCURRENT ELIMINATION\n");/* DELETE_FROM_QUEUE */
						unlock(lp_idx);
						return 0;
					}
					else {
						break;
					}
				}
				///* VALID AND NOT EXECUTED AND LOCK TAKEN AND EXTRACTED *///
				else if(event->state == EXTRACTED)
				{
					if(
						event->timestamp < bound_ptr->timestamp ||
						(
							event->timestamp == bound_ptr->timestamp 
							&& event->tie_breaker <= bound_ptr->tie_breaker
						)
					)
					{
						unlock(lp_idx);
						add_lp_unsafe_set(lp_idx);
					}
					else
						break;
				}
				else if(event->state == ANTI_EVENT)
				{
					break;
				}
				else if(event->state == ELIMINATED)
				{
					delete(nbcalqueue, node);
					unlock(lp_idx);
				}
			///* VALID AND NOT EXECUTED AND LOCK NOT TAKEN *///

		}
		///* NOT VALID *///		
		else {
			///* NOT VALID AND ELIMINATED*///
			if((event->state & EXTRACTED) == 0){
				delete(nbcalqueue, node);
				unlock(lp_idx);
			}
			else{
				if((event->state & ELIMINATED) == 0){
					__sync_or_and_fetch(&event->state, ELIMINATED);
					unlock(lp_idx);
				}
				else{
					delete(nbcalqueue, node);
					break;
				}
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
    					printf("FETCH DONE 3\n");
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
