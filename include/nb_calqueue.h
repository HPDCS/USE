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

#include <float.h>
#include <math.h>
#include "atomic.h"
#include "core.h"
#include "lookahead.h"
#include "garbagecollector.h"
#include "events.h"

#define INFTY DBL_MAX

//#define LESS(a,b) 		( (a) < (b) && !D_EQUAL((a), (b)) )
//#define LEQ(a,b)		( (a) < (b) ||  D_EQUAL((a), (b)) )
//#define D_EQUAL(a,b) 	(fabs((a) - (b)) < DBL_EPSILON)
//#define GEQ(a,b) 		( (a) > (b) ||  D_EQUAL((a), (b)) )
//#define GREATER(a,b) 	( (a) > (b) &&  !D_EQUAL((a), (b)) )

#define LESS(a,b)            ( (a) <  (b)  )
#define LEQ(a,b)             ( (a) <= (b) )
#define D_EQUAL(a,b)         ( (a) == (b) )
#define GEQ(a,b)             ( (a) >= (b) )
#define GREATER(a,b)         ( (a) >  (b) )


#define SAMPLE_SIZE 25
#define HEAD_ID 0
#define MAXIMUM_SIZE 65536//32768 //65536
#define MINIMUM_SIZE 1

#define FLUSH_SMART 1
#define ENABLE_EXPANSION 1
#define ENABLE_PRUNE 1

#define TID tid

/**
 *  Struct that define a node in a bucket
 */
typedef struct __bucket_node nbc_bucket_node;
struct __bucket_node
{
	//char zpad1[64];
	nbc_bucket_node * volatile next;	// pointer to the successor
//	char zpad2[56];
	nbc_bucket_node * volatile replica;	// pointer to the replica
//	char zpad3[56];
	//volatile unsigned int to_remove; 			// used to resolve the conflict with same timestamp using a FIFO policy
	//char zpad[60];
	//void *generator;	// pointer to the successor
	//void *payload;  				// general payload
	msg_t *payload;  				// general payload
	double timestamp;  				// key
	unsigned long long epoch;		//enqueue's epoch
	unsigned int counter; 			// used to resolve the conflict with same timestamp using a FIFO policy
	//char zpad3[36];					// actually used only to distinguish head nodes
	unsigned int tag;
	bool reserved;
#if DEBUG == 1 // TODO
	unsigned int copy;
	unsigned int deleted;
	unsigned int executed;
#endif
};


/**
 *
 */

typedef struct table table;
struct table
{
	nbc_bucket_node * array;
	double bucket_width;
	unsigned int size;
	unsigned int pad;
	table * volatile new_table;
	char zpad4[32];
	atomic_t counter;
	//atomic_t e_counter;	//TODO: scompattare il counter
	//char zpad3[60];
	//atomic_t d_counter;
	char zpad1[60];
	volatile unsigned long long current;
	char zpad2[56];
};


typedef struct nb_calqueue nb_calqueue;
struct nb_calqueue
{
	unsigned int threshold;
	unsigned elem_per_bucket;
	double perc_used_bucket;
	double pub_per_epb;
	char zpad9[40];
	table * volatile hashtable;
};


extern __thread unsigned int TID;
extern __thread struct drand48_data seedT;
extern nbc_bucket_node *g_tail;

extern void nbc_enqueue(nb_calqueue *queue, double timestamp, void* payload, unsigned int tag);
extern void nbc_prune(void);
extern nb_calqueue* nb_calqueue_init(unsigned int threashold, double perc_used_bucket, unsigned int elem_per_bucket);

extern nbc_bucket_node* getMin(nb_calqueue *queue, table ** h);
extern nbc_bucket_node* getNext(nbc_bucket_node* node, table *h);
extern bool delete(nb_calqueue *queue, nbc_bucket_node* node);

extern void getMinLP_internal(unsigned int lp);
extern unsigned int getMinFree_internal();
extern unsigned int fetch_internal();
extern bool commit_event(msg_t * event, nbc_bucket_node * node, unsigned int lp_idx);


extern unsigned long long hash(double timestamp, double bucket_width);

#endif /* DATATYPES_NONBLOCKING_QUEUE_H_ */
