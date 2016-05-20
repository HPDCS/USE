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
 * nonblockingqueue.h
 *
 *  Created on: Jul 13, 2015
 *      Author: Romolo Marotta
 */
#ifndef DATATYPES_NONBLOCKING_CALQUEUE_H_
#define DATATYPES_NONBLOCKING_CALQUEUE_H_

#include <stdbool.h>
#include <float.h>
#include "atomic.h"

#define INFTY DBL_MAX
#define LESS(a,b) 		( (a) < (b) && !D_EQUAL((a), (b)) )
#define LEQ(a,b)		( (a) < (b) ||  D_EQUAL((a), (b)) )
#define D_EQUAL(a,b) 	(fabs((a) - (b)) < DBL_EPSILON)
#define GEQ(a,b) 		( (a) > (b) ||  D_EQUAL((a), (b)) )
#define GREATER(a,b) 	( (a) > (b) &&  !D_EQUAL((a), (b)) )
#define SAMPLE_SIZE 25
#define HEAD_ID 0
#define MAXIMUM_SIZE 65536//32768 //65536
#define MINIMUM_SIZE 1


#define ENABLE_EXPANSION 1
#define ENABLE_PRUNE 	 1
#define FLUSH_SMART		 1


extern __thread unsigned int  lid;


/**
 *  Struct that define a node in a bucket
 */
typedef struct __bucket_node nbc_bucket_node;
struct __bucket_node
{
	//char zpad1[64];
	nbc_bucket_node * volatile next;	// pointer to the successor
	char zpad2[56];
	nbc_bucket_node * volatile replica;	// pointer to the replica
	char zpad3[56];
	//volatile unsigned int to_remove; 			// used to resolve the conflict with same timestamp using a FIFO policy
	//char zpad[60];
	//void *generator;	// pointer to the successor
	void *payload;  				// general payload
	double timestamp;  				// key
	unsigned int counter; 			// used to resolve the conflict with same timestamp using a FIFO policy
	//char zpad3[36];					// actually used only to distinguish head nodes
};


/**
 *
 */

typedef struct table table;
struct table
{
	nbc_bucket_node * array;
	char zpad1[56];
	table * volatile new_table;
	char zpad2[56];
	atomic_t counter;
	char zpad3[60];
	volatile unsigned long long current;
	char zpad4[56];
	unsigned int size;
	double bucket_width;
};


typedef struct nb_calqueue nb_calqueue;
struct nb_calqueue
{
	unsigned int threshold;
	char zpad9[56];
	table * volatile hashtable;
};

extern void nbc_enqueue(nb_calqueue *queue, double timestamp, void* payload);
extern nbc_bucket_node* nbc_dequeue(nb_calqueue *queue);
extern double nbc_prune(double timestamp);
extern nb_calqueue* nb_calqueue_init(unsigned int threashold);

#endif /* DATATYPES_NONBLOCKING_QUEUE_H_ */
