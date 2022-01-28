#pragma once
#ifndef __EVENTS_H
#define __EVENTS_H

#include <stdbool.h>
#include "../reverse/reverse.h"

#define MAX_DATA_SIZE		128

#define NEW_EVT 	0x0
#define EXTRACTED 	0x1
#define ELIMINATED	0x2
#define ANTI_MSG	0x3


#define EVT_SAFE   ((void*)0x5AFE)
#define EVT_BANANA ((void*)0xBA4A4A)


#define CONCURRENT(x,y)    		   ( (x->timestamp == y->timestamp && x->tie_breaker<=y->tie_breaker)  )

#define BEFORE(x,y)  			   ( (x->timestamp< y->timestamp) || (x->timestamp == y->timestamp && x->tie_breaker< y->tie_breaker)  ) 
#define BEFORE_OR_CONCURRENT(x,y)  ( (x->timestamp< y->timestamp) || (x->timestamp == y->timestamp && x->tie_breaker<=y->tie_breaker)  ) 
#define AFTER_OR_CONCURRENT(x,y)   ( (x->timestamp> y->timestamp) || (x->timestamp == y->timestamp && x->tie_breaker>=y->tie_breaker)  ) 
#define AFTER(x,y)  			   ( (x->timestamp> y->timestamp) || (x->timestamp == y->timestamp && x->tie_breaker> y->tie_breaker)  ) 


/// This defines the type with whom timestamps are represented
typedef double simtime_t;



typedef struct __msg_t
{
	/* event's attributes */
	unsigned short sender_id;//unsigned int sender_id;						//MAI				//Sednder LP
	unsigned short receiver_id;//unsigned int receiver_id;					//P-F				//Receiver LP
	
	simtime_t timestamp;						//P-F				//Timestamp execution of the event
	
	unsigned short tie_breaker;//unsigned long long tie_breaker; 			//F					//maximum timestamp of produced events used for garbage collection of msg_t due to lazy invalidation 
	
	char type;//int type;									//P					//Type of event (e.g. INIT_STATE)
	char data_size;//unsigned int data_size;									//size of the payload of the vent
	
	/*volatile*/ unsigned int epoch;			//F					//LP's epoch at executing time		
	unsigned int frame; 						//F					//debug//order of execution of the event in the tymeling
	
	void * monitor;//unsigned int monitor;//								//F			
					
					
	/* validity attributes */				
	
	struct __msg_t * father;					//F					//address of the father event
	
	simtime_t max_outgoing_ts; 					//ALTRO				//maximum timestamp of produced events used for garbage collection of msg_t due to lazy invalidation 
		
	unsigned int fatherFrame;	 				//F					//order of execution of the father in the tymeling	
	/*volatile*/ unsigned int fatherEpoch;		//ALTRO			//father LP's epoch at executing time
	
		
			
	/*volatile*/ unsigned int state;				//F			//state of the node (EXTRACTED, ELIMINATED OR ANTI-EVENT)
		
	unsigned char data[MAX_DATA_SIZE];						//payload of the event

	
	
#if DEBUG==1 //REVERSIBLE
	/* Support to undo event mechanism */ 
	unsigned int previous_seed;	//seed to generate random number taken before the execution
#endif
	revwin_t *revwin;			//reverse window to rollback
#if DEBUG == 1		
	struct __bucket_node * node;					//address of the belonging node

	unsigned int roll_epoch;	//DEBUG

	struct __bucket_node * del_node;
	
	unsigned long long creation_time;
	unsigned long long execution_time;
	unsigned long long rollback_time;
	unsigned long long deletion_time;

	struct __msg_t * local_next;	//address of the next event executed on the relative LP //occhio, potrebbe non servire
	struct __msg_t * local_previous;	//address of the previous event executed on the relative LP ////occhio, potrebbe non servire
	struct __msg_t * commit_bound;	//address of the previous event executed on the relative LP ////occhio, potrebbe non servire

	simtime_t gvt_on_commit;
	struct __msg_t * event_on_gvt_on_commit;
#endif

} msg_t;


typedef struct _msg_hdr_t {
	// Kernel's information
	unsigned int   		sender;
	unsigned int   		receiver;
	// TODO: not required, remove
	int   			type;
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous event
	// TODO: remove until here
	simtime_t		timestamp;
	simtime_t		send_time;
	unsigned long long	mark;
} msg_hdr_t;

typedef struct _outgoing_t {
	msg_t **outgoing_msgs;
	unsigned int size;
	unsigned int max_size;
	simtime_t *min_in_transit;
} outgoing_t;


#define set_commit_state_as_banana(event)				(	EVT_TRANSITION_ANT_BAN(event)  	) 

static inline bool EVT_TRANSITION_ANT_BAN(msg_t *event){
	event->monitor =  (void*) EVT_BANANA;
	return true;
}

static inline bool EVT_TRANSITION_NEW_EXT(msg_t *event){
	int res = __sync_or_and_fetch(&(event->state),EXTRACTED) == EXTRACTED;
	
	//or it was eliminated, or it was and antimessage in the future...in any case it has to be considered as executed (?)
	
	// the event was NEW_EVT and after the extraction is not EXTRACTED
	// consequently it is now ANTI
	// regardless if it is in the present or in the past it has been annihilited before it was executed
	// so consider its rollback as executed
	if(!res) EVT_TRANSITION_ANT_BAN(event);

	return res;
}


static inline bool EVT_TRANSITION_NEW_ELI(msg_t *event){
	int res = __sync_or_and_fetch(&(event->state),ELIMINATED) == ELIMINATED;
	return res;
}

static inline bool EVT_TRANSITION_EXT_ANT(msg_t *event){
	int res = __sync_or_and_fetch(&(event->state),ELIMINATED) == ELIMINATED;
	return res;
}


// the transition ELI to ANTI should never be called explicitely
//#define EVT_TRANSITION_ELI_ANT(x) {}









#endif
