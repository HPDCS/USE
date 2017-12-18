#pragma once
#ifndef __SIMTYPES_H
#define __SIMTYPES_H

#include <float.h>
#include <atomic.h>


/// Infinite timestamp: this is the highest timestamp in a simulation run
#define INFTY DBL_MAX


/// This defines the type with whom timestamps are represented
typedef double simtime_t;


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
} msg_t;

typedef struct _LP_state {
#ifdef ENABLE_ULT
	/// LP execution state. This MUST be the first declared field in the struct
	LP_context_t	context;

	/// LP execution state when blocked during the execution of an event
	LP_context_t	default_context;

	/// Process' stack
	void 		*stack;
#endif /* ENABLE_ULT */

	/// Local ID of the thread (used to translate from bound LPs to local id)
	unsigned int 	lid;

	/// Logical Process lock, used to serialize accesses to concurrent data structures
	spinlock_t	lock;

	/// Seed to generate pseudo-random values
	seed_type	seed;


	/// ID of the worker thread towards which the LP is bound
	unsigned int	worker_thread;

	/// Current execution state of the LP
	short unsigned int state;

	/// This variable mainains the current checkpointing interval for the LP
	unsigned int	ckpt_period;

	/// Counts how many events executed from the last checkpoint (to support PSS)
	unsigned int	from_last_ckpt;

	/// If this variable is set, the next invocation to LogState() takes a new state log, independently of the checkpointing interval
	bool		state_log_forced;

	/// The current state base pointer (updated by SetState())
	void 		*current_base_pointer;

	/// Input messages queue
	list(msg_t)	queue_in;

	/// Pointer to the last correctly elaborated event
	msg_t		*bound;

	/// Output messages queue
	list(msg_hdr_t)	queue_out;

	/// Saved states queue
	list(state_t)	queue_states;

	/// Bottom halves queue
	list(msg_t)	bottom_halves;

	/// Processed rendezvous queue
	list(msg_t)	rendezvous_queue;

	/// Unique identifier within the LP
	unsigned long long	mark;

	/// Buffer used by KLTs for buffering outgoing messages during the execution of an event
	outgoing_t outgoing_buffer;

	#ifdef HAVE_CROSS_STATE
	unsigned int ECS_synch_table[MAX_CROSS_STATE_DEPENDENCIES];
	unsigned int ECS_index;
	#endif

	unsigned long long	wait_on_rendezvous;
	unsigned int		wait_on_object;

} LP_state;


#endif
