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
#include <string.h>
#include <sys/types.h>
#include <float.h>
#include <pthread.h>
#include <math.h>

#include "atomic.h"
#include "nb_calqueue.h"
#include "core.h"

__thread nbc_bucket_node *to_free_nodes = NULL;
__thread nbc_bucket_node *to_free_nodes_old = NULL;

__thread nbc_bucket_node *to_free_tables_old = NULL;
__thread nbc_bucket_node *to_free_tables_new = NULL;

__thread unsigned int to_remove_nodes_count = 0;
__thread unsigned int  mark;
__thread unsigned int  prune_count = 0;

static unsigned int * volatile prune_array;
static unsigned int threads;

static nbc_bucket_node *g_tail;

#define VAL ((unsigned long long)0)
#define DEL ((unsigned long long)1)
#define MOV ((unsigned long long)2)
#define INV ((unsigned long long)3)

#define REMOVE_DEL		 0
#define REMOVE_DEL_INV	 1


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
static inline unsigned int hash(double timestamp, double bucket_width)
{
	unsigned int res =  (unsigned int) (timestamp / bucket_width);

	if(timestamp / bucket_width > pow(2, 32))
		error("Probable Overflow when computing the index: "
				"TS=%e,"
				"BW:%e, "
				"TS/BW:%e, "
				"2^32:%e\n",
				timestamp, bucket_width, timestamp / bucket_width,  pow(2, 32));


	unsigned int ret = 0;
	bool end = false;
	unsigned int count = 0;

	res -= (res & (-(res < 3))) +  (3 & (-(res >= 3)));

	while( !end )
	{
		double tmp1 = res*bucket_width;

		ret = ((res) & (-(D_EQUAL(tmp1, timestamp)))) +  ((res-1) & (-(GREATER(tmp1, timestamp))));
		end = D_EQUAL(tmp1, timestamp) || GREATER(tmp1, timestamp);

		if(res+1 < res)
			error("Overflow when computing the index\n");
		res++;

		count++;
	}

	return ret;
}

/**
 *  This function returns an unmarked reference
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return the unmarked value of the pointer
 */
static inline void* get_unmarked(void *pointer)
{
	return (void*) (((unsigned long long) pointer) & ~((unsigned long long) 3));
}

/**
 *  This function returns a marked reference
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return the marked value of the pointer
 */
static inline void* get_marked(void *pointer, unsigned long long mark)
{
	return (void*) (((unsigned long long) get_unmarked((pointer))) | (mark));
}

/**
 *  This function checks if a reference is marked
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return true if the reference is marked, else false
 */
static inline bool is_marked(void *pointer, unsigned long long mask)
{
	return (bool) ((((unsigned long long) pointer) &  ((unsigned long long) 3)) == mask);
}

static inline bool is_marked_for_search(void *pointer, unsigned int mask)
{
	if(mask == REMOVE_DEL)
		return (bool) ((((unsigned long long) pointer) &  ((unsigned long long) 3)) == DEL);
	if(mask == REMOVE_DEL_INV)
	{		bool a = (bool) ((((unsigned long long) pointer) &  ((unsigned long long) 3)) == DEL);
			bool b = (bool) ((((unsigned long long) pointer) &  ((unsigned long long) 3)) == INV);
			return a || b;
	}
	return false;
}

/**
* This function generates a mark value that is unique w.r.t. the previous values for each Logical Process.
* It is based on the Cantor Pairing Function, which maps 2 naturals to a single natural.
* The two naturals are the LP gid (which is unique in the system) and a non decreasing number
* which gets incremented (on a per-LP basis) upon each function call.
* It's fast to calculate the mark, it's not fast to invert it. Therefore, inversion is not
* supported at all in the code.
*
* @author Alessandro Pellegrini
*
* @param tid The local Id of the Light Process
* @return A value to be used as a unique mark for the message within the LP
*/
static inline unsigned long long generate_ABA_mark() {
	unsigned long long k1 = tid;
	unsigned long long k2 = mark++;
	unsigned long long res = (unsigned long long)( ((k1 + k2) * (k1 + k2 + 1) / 2) + k2 );
	return ((~((unsigned long long)0))>>32) & res;
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
	nbc_bucket_node* res = (nbc_bucket_node*) malloc(sizeof(nbc_bucket_node));

	if (is_marked(res, DEL) || is_marked(res, MOV) || is_marked(res, INV) || res == NULL)
	{
		error("%lu - Not aligned Node or No memory\n", pthread_self());
		abort();
	}

	res->counter = tie_breaker;
	res->next = NULL;
	res->replica = NULL;
	res->payload = payload;
	res->timestamp = timestamp;

	return res;
}

/**
 * This function connect to a private structure marked
 * nodes in order to free them later, during a synchronisation point
 *
 * @author Romolo Marotta
 *
 * @param queue used to associate freed nodes to a queue
 * @param start the pointer to the first node in the disconnected sequence
 * @param end   the pointer to the last node in the disconnected sequence
 * @param timestamp   the timestamp of the last disconnected node
 *
 *
 */
static inline void connect_to_be_freed_node_list(nbc_bucket_node *start, unsigned int counter)
{
	nbc_bucket_node* new_node;
	start = get_unmarked(start);

	new_node = node_malloc(start, start->timestamp, counter);
	new_node->next = to_free_nodes;

	to_free_nodes = new_node;
	to_remove_nodes_count += counter;
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
 * @param right_node a pointer to a pointer used to return the right node
 *
 */

static void search(nbc_bucket_node *head, double timestamp, unsigned int tie_breaker,
						nbc_bucket_node **left_node, nbc_bucket_node **right_node, int flag)
{
	nbc_bucket_node *left, *right, *left_next, *tmp, *tmp_next, *tail;
	nbc_bucket_node *right_unmarked;
	unsigned int counter;
	tail = g_tail;

	do
	{
		/// Fetch the head and its next node
		tmp = head;
		tmp_next = tmp->next;
		counter = 0;
		double tmp_timestamp;
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
			//if (tmp == tail)
			//	break;
			tmp_next = tmp->next;

			// Exit if tmp is a tail or its timestamp is > of the searched key
		} while (	tmp != tail &&
					(
						is_marked_for_search(tmp_next, flag)
						|| ( LEQ(tmp_timestamp, timestamp) && tie_breaker == 0)
						|| ( LEQ(tmp_timestamp, timestamp) && tie_breaker != 0 && tmp->counter <= tie_breaker)
					)
				);

		// Set right node
		right = tmp;
		right_unmarked = tmp;

		//left node and right node have to be adjacent. If not try with CAS
		if (get_unmarked(left_next) != right)
		{
			if(is_marked(left_next, MOV))
				right = get_marked(right, MOV);

			else if(flag == REMOVE_DEL && is_marked(left_next, INV))
				right = get_marked(right, INV);
			// if CAS succeeds connect the removed nodes to to_be_freed_list
			if (!CAS_x86(
						(volatile unsigned long long *)&(left->next),
						(unsigned long long) left_next,
						(unsigned long long) right
						)
					)
				continue;
			connect_to_be_freed_node_list(left_next, counter);
		}
		// at this point they are adjacent. Thus check that right node is still unmarked and return
		if (right_unmarked == tail || !is_marked_for_search(right_unmarked->next, flag))
		{
			*left_node = left;
			*right_node = right;
			if(flag == REMOVE_DEL_INV )
				*right_node = right_unmarked;
			
			return;
		}
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
	unsigned long long oldCur;
	unsigned int oldIndex;
	unsigned int index = hash(node->timestamp, h->bucket_width);
	unsigned long long newCur =  ( ( unsigned long long ) index ) << 32;
	newCur |= generate_ABA_mark();

	// Retry until it is ensured that the current is moved back to index
#if FLUSH_SMART == 1
	oldCur = h->current;
	oldIndex = (unsigned int) (oldCur >> 32);
	if(
		index > oldIndex || is_marked(node->next, DEL)
		|| CAS_x86(
					(volatile unsigned long long *)&(h->current),
					(unsigned long long) oldCur,
					(unsigned long long) newCur
					)
				) return;
#endif

	do
	{

		oldCur = h->current;
		oldIndex = (unsigned int) (oldCur >> 32);
	}
	while (
			index <
#if FLUSH_SMART == 0
			=
#endif
			oldIndex && !is_marked(node->next, DEL)
			&& !CAS_x86(
						(volatile unsigned long long *)&(h->current),
						(unsigned long long) oldCur,
						(unsigned long long) newCur
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


	switch(flag)
	{
	case REMOVE_DEL_INV:

		new_node_pointer->next = right_node;
		// set tie_breaker
		new_node_pointer->counter = 1 + ( -D_EQUAL(new_node_timestamp, left_node->timestamp ) & left_node->counter );

		if (CAS_x86(
					(volatile unsigned long long*)&(left_node->next),
					(unsigned long long) right_node,
					(unsigned long long) new_node_pointer
				))
			return true;

		// reset tie breaker
		new_node_pointer->counter = 0;
		break;

	case REMOVE_DEL:

		new_node_pointer->next = get_marked(right_node, INV);
		// node already exists
		if(D_EQUAL(new_node_timestamp, left_node->timestamp ) && left_node->counter == new_node_counter)
		{
			free(new_node_pointer);
			*new_node = left_node;
			return true;
		}
		// first replica to be inserted
		if(is_marked(right_node, INV))
			new_node_pointer = get_marked(new_node_pointer, INV);

		if (CAS_x86(
					(volatile unsigned long long*)&(left_node->next),
					(unsigned long long) right_node,
					(unsigned long long) new_node_pointer
				))
			//printf("insert_std: evt type %u\n", ((msg_t*)(new_node[0]->payload))->type);//da_cancellare
			return true;
		break;
	}
	return false;

}

static void set_new_table(table* h, unsigned int threshold )
{
	nbc_bucket_node *tail = g_tail;
	int signed_counter = atomic_read( &h->counter );
	signed_counter = (-(signed_counter >= 0)) & signed_counter;


	unsigned int counter = (unsigned int)signed_counter;
	unsigned int i = 0;
	unsigned int size = h->size;
	unsigned int thp2 = threshold *2;
	unsigned int new_size = 0;


	if		(size == 1 && counter > thp2)
		new_size = thp2;
	else if (size == thp2 && counter < threshold)
		new_size = 1;
	else if (size >= thp2 && counter > 2*size)
		new_size = 2*size;
	else if (size > thp2 && counter < 0.5*size)
		new_size =  0.5*size;

	if(new_size > MAXIMUM_SIZE )
		new_size = 0;

	if(new_size > 0)
	{

		table *new_h = (table*) malloc(sizeof(table));
		if(new_h == NULL)
			error("No enough memory to new table structure\n");

		new_h->bucket_width = -1.0;
		new_h->size 		= new_size;
		new_h->new_table 	= NULL;
		new_h->counter.count= 0;
		new_h->current 		= ((unsigned long long)-1) << 32;


		nbc_bucket_node *array =  (nbc_bucket_node*) malloc(sizeof(nbc_bucket_node) * new_size);
		if(array == NULL)
		{
			free(new_h);
			error("No enough memory to allocate new table array %u\n", new_size);
		}

		memset(array, 0, sizeof(nbc_bucket_node) * new_size);

		for (i = 0; i < new_size; i++)
			array[i].next = tail;


		new_h->array 	= array;

		if(!CAS_x86(
				(volatile unsigned long long *) &(h->new_table),
				(unsigned long long)			NULL,
				(unsigned long long) 			new_h))
		{
			free(new_h->array);
			free(new_h);
		}

		//else
		//	printf("CHANGE SIZE from %u to %u, items %u #\n", size, new_size, counter);
	}
}

static void block_table(table* h, unsigned int *index, double *min_timestamp)
{
	unsigned int i=0;
	unsigned int size = h->size;
	nbc_bucket_node *tail = g_tail;
	nbc_bucket_node *array = h->array;
	*min_timestamp = INFTY;

	for(i = 0; i < size; i++)
	{
		nbc_bucket_node *bucket = array + i;
		nbc_bucket_node *bucket_next, *right_node, *left_node, *right_node_next;

		bool marked = false;
		bool cas_result = false;

		do
		{
			bucket_next = bucket->next;
			marked = is_marked(bucket_next, MOV);
			if(!marked)
				cas_result = CAS_x86(
						(volatile unsigned long long *) &(bucket->next),
						(unsigned long long)			bucket_next,
						(unsigned long long) 			get_marked(bucket_next, MOV)
					);
		}
		while( !marked && !cas_result );

		do
		{
			search(bucket, 0.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
			right_node = get_unmarked(right_node);
			right_node_next = right_node->next;
		}
		while(
				right_node != tail &&
				(
					is_marked(right_node_next, DEL) ||
					is_marked(right_node_next, INV) ||
					(	get_unmarked(right_node_next) == right_node_next && !(
							cas_result = CAS_x86(
							(volatile unsigned long long *) &(right_node->next),
							(unsigned long long)			right_node_next,
							(unsigned long long) 			get_marked(right_node_next, MOV)
							))
					)
				)
			);

		if(LESS(right_node->timestamp, *min_timestamp) )
		{
			*min_timestamp = right_node->timestamp;
			*index = i;
		}

	}
}

static double compute_mean_separation_time(table* h,
		unsigned int new_size, unsigned int threashold,
		unsigned int index, double min_timestamp)
{
	nbc_bucket_node *tail = g_tail;

	unsigned int i;

	table *new_h = h->new_table;
	nbc_bucket_node *array = h->array;
	double old_bw = h->bucket_width;
	unsigned int size = h->size;
	double new_bw = new_h->bucket_width;

	if(new_bw < 0)
	{

		unsigned int sample_size;
		double average = 0.0;
		double newaverage = 0.0;

		if(new_size <= threashold*2)
			newaverage = 1.0;
		else
		{

			if (new_size <= 5)
				sample_size = new_size/2;
			else
				sample_size = 5 + (int)((double)new_size / 10);

			if (sample_size > SAMPLE_SIZE)
				sample_size = SAMPLE_SIZE;

			double sample_array[sample_size];

			int counter = 0;
			i = 0;

			min_timestamp = hash(min_timestamp, old_bw)*old_bw;

			double min_next_round = INFTY;
			unsigned int new_index = 0;
			unsigned int limit = h->size*3;

			while(counter != sample_size && i < limit && new_h->bucket_width == -1.0)
			{

				nbc_bucket_node *tmp = array + (index+i)%size;
				nbc_bucket_node *tmp_next = get_unmarked(tmp->next);
				double tmp_timestamp = tmp->timestamp;

				while( tmp != tail && counter < sample_size &&  LESS(tmp_timestamp, min_timestamp + old_bw*(i+1)) )
				{

					tmp_next = tmp->next;
					if(
							!is_marked(tmp_next, DEL) &&
							!is_marked(tmp_next, INV) &&
							tmp->counter != HEAD_ID   &&
							GEQ(tmp_timestamp, min_timestamp + old_bw*(i)) &&
							!D_EQUAL(tmp_timestamp, sample_array[counter-1])
					)
						sample_array[counter++] = tmp_timestamp;
					else
					{
						if( LESS(tmp_timestamp, min_next_round) && GEQ(tmp_timestamp, min_timestamp + old_bw*(i+1)))
						{
							min_next_round = tmp_timestamp;
							new_index = i;
						}
					}
					tmp = get_unmarked(tmp_next);
					tmp_timestamp = tmp->timestamp;
				}
				i++;
				if(counter < sample_size && i >= size*3 && min_next_round != INFTY && new_h->bucket_width == -1.0)
				{
					min_timestamp = hash(min_next_round, old_bw)*old_bw;
					index = new_index;
					limit += size*3;
					min_next_round = INFTY;
					new_index = 0;
				}
			}
			if( counter < sample_size)
				sample_size = counter;

			for(i = 1; i<sample_size;i++)
				average += sample_array[i] - sample_array[i - 1];

				// Get the average
			average = average / (double)(sample_size - 1);

			int j=0;
			// Recalculate ignoring large separations
			for (i = 1; i < sample_size; i++) {
				if ((sample_array[i] - sample_array[i - 1]) < (average * 2.0))
				{
					newaverage += (sample_array[i] - sample_array[i - 1]);
					j++;
				}
			}

			// Compute new width
			newaverage = (newaverage / j) * 3.0;	/* this is the new width */
			if(newaverage <= 0.0)
				newaverage = 1.0;
			//if(newaverage <  pow(10,-4))
			//	newaverage = pow(10,-3);
			if(isnan(newaverage))
				newaverage = 1.0;
		}

		new_bw = newaverage;

	}
	return new_bw;
}

static table* read_table(nb_calqueue* queue)
{
	table *h = queue->hashtable;
#if ENABLE_EXPANSION == 0
	return h;
#endif
	nbc_bucket_node *tail = g_tail;

	//printf("SIZE H %d\n", h->counter.count);

	unsigned int i, index;
	if(h->new_table == NULL)
		set_new_table(h, queue->threshold);

	if(h->new_table != NULL)
	{
		//printf("%u - MOVING BUCKETS\n", tid);
		table *new_h = h->new_table;
		nbc_bucket_node *array = h->array;
		double new_bw = new_h->bucket_width;
		double min_timestamp = INFTY;

		if(new_bw < 0)
		{
			block_table(h, &index, &min_timestamp);
			double newaverage = compute_mean_separation_time(h, new_h->size, queue->threshold, index, min_timestamp);
			if
			(
					CAS_x86(
							(volatile unsigned long long *) &(new_h->bucket_width),
							*( (unsigned long long*)  &new_bw),
							*( (unsigned long long*)  &newaverage)
					)
				)
			{
				//	printf("COMPUTE BW %.20f %u\n", newaverage, new_h->size)

				;
			}
		}

		for(i=0; i < h->size; i++)
		{
			nbc_bucket_node *bucket = array + i;
			nbc_bucket_node *right_node, *left_node, *right_node_next;

			do
			{
				do
				{

					search(bucket, 0.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
					right_node = get_unmarked(right_node);
					right_node_next = right_node->next;
				}
				while(
						right_node != tail &&
						(
							is_marked(right_node_next, DEL) ||
							is_marked(right_node_next, INV) ||
							(	get_unmarked(right_node_next) == right_node_next && !(
									CAS_x86(
									(volatile unsigned long long *) &(right_node->next),
									(unsigned long long)			right_node_next,
									(unsigned long long) 			get_marked(right_node_next, MOV)
									))
							)
						)
					);

				if(right_node != tail)
				{
					nbc_bucket_node *replica = node_malloc(right_node->payload, right_node->timestamp,  right_node->counter);
					nbc_bucket_node *right_replica_field;
					nbc_bucket_node *replica2;

					if(replica == tail)
						error("A\n");

					replica2 = replica;
					while(!insert_std(new_h, &replica2, REMOVE_DEL) && queue->hashtable == h);

					if(queue->hashtable != h)
						return queue->hashtable;

					if(replica2 == tail)
						error("B %p %p %p\n", replica, replica2, tail);

					if(CAS_x86(
							(volatile unsigned long long *) &(right_node->replica),
							(unsigned long long)			NULL,
							(unsigned long long) 			replica2))
						atomic_inc_x86(&(new_h->counter));

					bool cas_result = false;
					bool marked  = false;
					right_replica_field = right_node->replica;

					if(right_replica_field == tail)
						error("%u - C %p %p %p %p %p\n", tid, replica, replica2, tail, right_replica_field, right_node->replica);

					if(replica == replica2 && replica != right_replica_field)
					{
						do
						{
							right_node_next = replica->next;
							marked = is_marked(right_node_next, DEL);
							if(!marked)
								cas_result = CAS_x86(
												(volatile unsigned long long *) &(replica->next),
												(unsigned long long)			right_node_next,
												(unsigned long long) 			get_marked(right_node_next, DEL)
											);

						}while( !marked && !cas_result ) ;
					}

					do
					{
						right_node_next = right_replica_field->next;
						marked = is_marked(right_node_next, INV);
						if(marked)
							cas_result = CAS_x86(
											(volatile unsigned long long *) &(right_replica_field->next),
											(unsigned long long)			right_node_next,
											(unsigned long long) 			get_unmarked(right_node_next)
										);
					}while( marked && !cas_result ) ;

					do{
						right_node_next = right_node->next;
						marked = is_marked(right_node_next, DEL);
						if(!marked)
							cas_result = CAS_x86(
											(volatile unsigned long long *) &(right_node->next),
											(unsigned long long)			right_node_next,
											(unsigned long long) 			get_marked(get_unmarked(right_node_next), DEL)
										);

					}while( !marked && !cas_result ) ;

					nbc_flush_current(
							new_h,
							right_replica_field);
				}
			}while(right_node != tail);
		}

		if(CAS_x86(
				(volatile unsigned long long *) &(queue->hashtable),
				(unsigned long long)			h,
				(unsigned long long) 			new_h
		))
			connect_to_be_freed_table_list(h);

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
nb_calqueue* nb_calqueue_init(unsigned int threshold)
{
	unsigned int i = 0;

	threads = threshold;
	prune_array = (unsigned int*) malloc(sizeof(unsigned int)*threshold*threshold);
	memset(prune_array, 0, sizeof(unsigned int)*threshold*threshold);

	nb_calqueue* res = (nb_calqueue*) malloc(sizeof(nb_calqueue));
	if(res == NULL)
		error("No enough memory to allocate queue\n");
	memset(res, 0, sizeof(nb_calqueue));

	res->threshold = threshold;

	res->hashtable = (table*) malloc(sizeof(table));
	if(res->hashtable == NULL)
	{
		free(res);
		error("No enough memory to allocate queue\n");
	}
	res->hashtable->bucket_width = 1.0;
	res->hashtable->new_table = NULL;
	res->hashtable->size = 1;

	res->hashtable->array = (nbc_bucket_node*) malloc(sizeof(nbc_bucket_node) );
	if(res->hashtable->array == NULL)
	{
		free(res->hashtable);
		free(res);
		error("No enough memory to allocate queue\n");
	}

	memset(res->hashtable->array, 0, sizeof(nbc_bucket_node) );

	g_tail = node_malloc(NULL, INFTY, 0);
	g_tail->next = NULL;

	for (i = 0; i < 1; i++)
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
void nbc_enqueue(nb_calqueue* queue, double timestamp, void* payload)
{
	nbc_bucket_node *new_node = node_malloc(payload, timestamp, 0);
	table *h;
	if(new_node->counter != 0)
		{
			// Should never be executed
			printf("LINE 933 NOT VALID %u new_node_counter\n", new_node->counter);
			exit(1);
		}

	//printf("ENQ %p %f %u\n", new_node, new_node->timestamp, new_node->counter);
	do
		h  = read_table(queue);
	while(!insert_std(h, &new_node, REMOVE_DEL_INV));

	nbc_flush_current(
			h,
			new_node);
	atomic_inc_x86(&(h->counter));
	
	//printf("nbc_enqueue: evt type %u\n", ((msg_t*)(new_node->payload))->type);//da_cancellare
}

static bool CAS_for_mark( nbc_bucket_node* right_node, nbc_bucket_node* right_node_next)
{
	return CAS_x86(
			(volatile unsigned long long *)&(right_node->next),
			(unsigned long long) get_unmarked(right_node_next),
			(unsigned long long) get_marked(right_node_next, DEL)
			);

}

static bool CAS_for_increase_cur(table* h, unsigned long long current, unsigned long long newCur)
{
	return CAS_x86(
			(volatile unsigned long long *)&(h->current),
			(unsigned long long)			current,
			(unsigned long long) 			newCur				);
}

/**
 * This function dequeue from the nonblocking queue. The cost of this operation when succeeds should be O(1)
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 *
 * @return a pointer to a node that contains the dequeued value
 *
 */
nbc_bucket_node* nbc_dequeue(nb_calqueue *queue)
{
	nbc_bucket_node *right_node, *min, *right_node_next, *res, *tail, *left_node;
	table * h;
	unsigned long long current;

	unsigned int index;
	unsigned int size;
	nbc_bucket_node *array;
	double right_timestamp;
	double bucket_width;


	tail = g_tail;
	res = NULL;
	do
	{
		h = read_table(queue);

		current = h->current;
		size = h->size;
		array = h->array;
		bucket_width = h->bucket_width;

		index = (unsigned int)(current >> 32);


		min = array + (index % size);

		search(min, 0.0, 0, &left_node, &right_node, REMOVE_DEL_INV);
		
		//printf("nbc_dequeue: -1 evt type %u\n", ((msg_t*)(right_node->payload))->type); //da_cancellare


		if(is_marked(min->next, MOV))
			continue;

		right_node_next = right_node->next;
		right_timestamp = right_node->timestamp;


		if(right_node == tail && size == 1 )
			return NULL;

		else if( LESS(right_timestamp, (index)*bucket_width) )
			nbc_flush_current(h, right_node);

		else if( GREATER(right_timestamp, (index+1)*bucket_width) )
		{
			if(index+1 < index)
				error("\nOVERFLOW index:%u  %.10f %u %f\n", index, bucket_width, size, right_timestamp);

			unsigned long long newCur =  ( ( unsigned long long ) index+1 ) << 32;
			newCur |= generate_ABA_mark();
//				CAS_x86(
//						(volatile unsigned long long *)&(h->current),
//						(unsigned long long)			current,
//						(unsigned long long) 			newCur				)
			CAS_for_increase_cur(h, current, newCur)

			;
		}
		else if(!is_marked(right_node_next, DEL))
		{
			unsigned int new_current = ((unsigned int)(h->current >> 32));
			if( new_current < index )
				continue;
			//printf("nbc_dequeue: 0 evt type %u\n", ((msg_t*)(right_node->payload))->type); //da_cancellare
			res = right_node->payload;
			
			//printf("nbc_dequeue: 1 evt type %u\n", ((msg_t*)(res->payload))->type); //da_cancellare
			if(
//					CAS_x86(
//								(volatile unsigned long long *)&(right_node->next),
//								(unsigned long long) get_unmarked(right_node_next),
//								(unsigned long long) get_marked(right_node_next, DEL)
//								)
					CAS_for_mark(right_node, right_node_next)
				)
			{
				nbc_bucket_node *left_node, *right_node;

				search(min, right_timestamp, 0,  &left_node, &right_node, REMOVE_DEL_INV);
				atomic_dec_x86(&(h->counter));

				//printf("nbc_dequeue: 2 evt type %u\n", ((msg_t*)(res->payload))->type); //da_cancellare
				return res;
			}
		}
//		else if(is_marked(right_node_next, DEL))
//		{
//				printf("IS MARKED "
//						"R:%p "
//						"R_N:%p "
//						"TIE:%u "
//						"TIME:%.10f "
//						"IS_INV:%u "
//						"CUR:%u "
//						"NUM:%u "
//						"TSIZE:%u "
//						"BW: %.10f "
//						"\n",
//						right_node,
//						right_node_next,
//						right_node->counter,
//						right_node->timestamp,
//						is_marked(right_node_next, INV),
//						index,
//						h->counter.count,
//						h->size,
//						h->bucket_width)		;
//		}
//		else if(right_node == tail )
//			error("NOOOOOOOOOOO\n");
//		}
		//else
		//	printf("NUM ELEMS %u %p %u %llu %llu\n", h->counter.count, h, index, current, current >> 32);

	}while(1);
	return NULL;
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
double nbc_prune(double timestamp)
{
#if ENABLE_PRUNE == 0
	return 0.0;
#endif
	unsigned int committed = 0;
	nbc_bucket_node **prec, *tmp, *tmp_next;
	nbc_bucket_node **meta_prec, *meta_tmp, *current_meta_node;
	unsigned int counter;
	unsigned int i=0;
	unsigned int flag = 1;

	prune_count++;

	if(prune_count < 100)
		return 0.0;
	else
		prune_count = 0;


	for(;i<threads;i++)
	{
		prune_array[tid + i*threads] = 1;
		flag &= prune_array[tid*threads +i];
	}

	if(flag == 1)
	{
		if(to_free_tables_old != NULL)
		{
//			if(to_free_tables_old->timestamp == INFTY)
//				to_free_tables_old->timestamp = timestamp;
//			else if(to_free_tables_old->timestamp < timestamp)
//			{
//				to_free_tables_old->counter++;
//				to_free_tables_old->timestamp = timestamp;
//			}
//			if(to_free_tables_old->counter > 3)
//			{
				while(to_free_tables_old != NULL)
				{
					nbc_bucket_node *my_tmp = to_free_tables_old;
					to_free_tables_old = to_free_tables_old->next;

					table *h = (table*) my_tmp->payload;
					free(h->array);
					free(h);
					free(my_tmp);
				}
//			}
		}
		else
		{
			to_free_tables_old = to_free_tables_new;
			to_free_tables_new = NULL;
		}


		if(to_free_nodes_old != NULL)
		{

			current_meta_node = to_free_nodes_old;
			meta_prec = (nbc_bucket_node**)&(to_free_nodes_old);

			while(current_meta_node != NULL)
			{
				//if(	LESS(current_meta_node->timestamp, timestamp) )
				{
					counter = current_meta_node->counter;
					prec = (nbc_bucket_node**)&(current_meta_node->payload);
					tmp = *prec;
					tmp = get_unmarked(tmp);

					while(counter-- != 0)
					{
						tmp_next = tmp->next;
						if(!is_marked(tmp_next, DEL) && !is_marked(tmp_next, INV))
							error("Found a %s node during prune "
									"NODE %p "
									"NEXT %p "
									"TAIL %p "
									"MARK %llu "
									"TS %f "
									"PRUNE TS %f "
									"TIE %u "
									"TIE META %u "
									"\n",
									is_marked(tmp_next, MOV) ? "MOV" : "VAL",
									tmp,
									tmp->next,
									g_tail,
									(unsigned long long) tmp->next & (3ULL),
									tmp->timestamp,
									timestamp,
									counter,
									current_meta_node->counter);

						//if( LESS(tmp->timestamp, timestamp))
						{
							current_meta_node->counter--;
							free(tmp);
							committed++;
						}
						tmp =  get_unmarked(tmp_next);
					}

					if(current_meta_node->counter == 0)
					{
						meta_tmp = current_meta_node;
						*meta_prec = current_meta_node->next;
						current_meta_node = current_meta_node->next;
						free(meta_tmp);
					}
					else
					{
						meta_prec = (nbc_bucket_node**) & ( current_meta_node->next );
						current_meta_node = current_meta_node->next;
					}
				}
//				else
//				{
//					meta_prec = (nbc_bucket_node**)  & ( current_meta_node->next );
//					current_meta_node = current_meta_node->next;
//				}
			}
			to_remove_nodes_count -= committed;
		}
		else
		{
			to_free_nodes_old = to_free_nodes;
			to_free_nodes = NULL;
		}


		for(i=0;i<threads;i++)
			prune_array[tid*threads +i] = 0;


	}
	return (double)committed;
}

