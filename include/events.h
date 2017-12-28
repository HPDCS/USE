#pragma once
#ifndef __EVENTS_H
#define __EVENTS_H

#define MAX_DATA_SIZE		128

typedef struct __msg_t
{
  unsigned int sender_id;	//Sednder LP
  unsigned int receiver_id;	//Receiver LP
  simtime_t timestamp;
  int type;
  unsigned int data_size;
  unsigned char data[MAX_DATA_SIZE];
  revwin_t *revwin;			//reverse window to rollback
  struct __bucket_node * node;	//address of the belonging node
  struct __msg_t * father;	//address of the belonging node
  unsigned long long fatherFrame;	
  unsigned long long fatherEpoch;	
  unsigned long long epoch;			
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
