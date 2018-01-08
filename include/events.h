#pragma once
#ifndef __EVENTS_H
#define __EVENTS_H

#define MAX_DATA_SIZE		128

#define EXTRACTED 	0x1
#define ELIMINATED	0x2
#define ANTI_EVENT	0x3

typedef struct __msg_t
{
	/* event's attributes */
	unsigned int sender_id;		//Sednder LP
	unsigned int receiver_id;	//Receiver LP
	simtime_t timestamp;		//Timestamp execution of the event
	int type;					//Type of event (e.g. INIT_STATE)
	unsigned int data_size;		//size of the payload of the vent
	unsigned char data[MAX_DATA_SIZE];	//payload of the event
	
	struct __bucket_node * node;	//address of the belonging node
	
	/* Support to undo event mechanism */ 
	revwin_t *revwin;			//reverse window to rollback
	unsigned int previous_seed;	//seed to generate random number taken before the execution
	
	/* validity attributes */
	unsigned int state;	//state of the node (EXTRACTED, ELIMINATED OR ANTI-EVENT)
	unsigned long long epoch;	//LP's epoch at executing time		
	//unsigned long long executed_frame; //order of execution of the event in the tymeling
	struct __msg_t * father;	//address of the father event
	unsigned long long fatherFrame; //order of execution of the father in the tymeling	
	unsigned long long fatherEpoch; //father LP's epoch at executing time
	simtime_t max_outgoing_ts; //maximum timestamp of produced events used for garbage collection of msg_t due to lazy invalidation 
	
	struct __msg_t * local_next;	//address of the next event executed on the relative LP //occhio, potrebbe non servire
	struct __msg_t * local_previous;	//address of the previous event executed on the relative LP ////occhio, potrebbe non servire
	
	//struct state_t * previous_checkpoint; //the last checkpoint taken before the execution of the current event
		
} msg_t;

typedef struct _msg_hdr_t {
	// Kernel's information
	unsigned int   		sender;
	unsigned int   		receiver;
	// TODO: non serve davvero, togliere
	int   			type;
	unsigned long long	rendezvous_mark;	/// Unique identifier of the message, used for rendez-vous event
	// TODO: fine togliere
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

#endif
