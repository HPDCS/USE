#include <ROOT-Sim.h>



/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORM		0
#define EXPONENTIAL	1
#define DISTRIBUTION	1


#define CHECK_FADING_TIME	10

#ifndef COMPLETE_CALLS
#define COMPLETE_CALLS		5000
#endif

#ifndef TA
#define TA			0.4
#endif


#define TA_DURATION		120
//#define TA_DURATION		60

#ifndef NUM_HOT
#define NUM_HOT 0
#endif


#define PARTITIONS 2
#define CELLS_PER_PARTITION  (n_prc_tot/PARTITIONS)

#define PERC_HOT 0.2
#define NUM_HOT_CELLS  ((unsigned int)(PERC_HOT*n_prc_tot))

#define NUM_CLD_CELLS_IN_MAX ((n_prc_tot/PARTITIONS)-NUM_HOT_CELLS)
#define LOAD_FROM_CLD_CELLS  (NUM_CLD_CELLS_IN_MAX/TA)

#define TARGET_SKEW 0.5
#define MIN_LOAD_PARTITION	   (CELLS_PER_PARTITION/TA)
#define MAX_LOAD_PARTITION	   (MIN_LOAD_PARTITION*(1+TARGET_SKEW))
#define LOAD_FROM_HOT_CELLS    (MAX_LOAD_PARTITION-LOAD_FROM_CLD_CELLS)
#define TA_HOT                 (NUM_HOT_CELLS/LOAD_FROM_HOT_CELLS)

#if NUM_HOT != 0
#define CHANNELS_PER_HOT_CELL	((unsigned int)(TA_DURATION*1.1/TA_HOT))
#endif


#ifndef CHANNELS_PER_CELL
#define CHANNELS_PER_CELL	1000
#endif

#ifndef TA_CHANGE
#define TA_CHANGE		300.0
#endif




//#define TA_CHANGE		75.0//150.0

#define	CELL_CHANGE_DISTRIBUTION	EXPONENTIAL
#define DURATION_DISTRIBUTION		EXPONENTIAL

#define HANDOFF_SHIFT 0.000001


/* Channel states */
#define CHAN_BUSY	1
#define CHAN_FREE	0

/* EVENT TYPES - PCS */
#define START_CALL	20
#define END_CALL	21
#define HANDOFF_LEAVE	30
#define HANDOFF_RECV	31
#define FADING_RECHECK	40

#define FADING_RECHECK_FREQUENCY	300	// Every 5 Minutes

#define MSK 0x1
#define SET_CHANNEL_BIT(B,K) ( B |= (MSK << K) )
#define RESET_CHANNEL_BIT(B,K) ( B &= ~(MSK << K) )
#define CHECK_CHANNEL_BIT(B,K) ( B & (MSK << K) )

#define BITS (sizeof(int) * 8)

#define CHECK_CHANNEL(P,I) ( CHECK_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )
#define SET_CHANNEL(P,I) ( SET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )
#define RESET_CHANNEL(P,I) ( RESET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) )

// Message exchanged among LPs
typedef struct _event_content_type {
	int cell; // The destination cell of an event
	unsigned int from; // The sender of the event (in case of HANDOFF_RECV)
	simtime_t sent_at; // Simulation time at which the call was handed off
	int channel; // Channel to be freed in case of END_CALL
	simtime_t   call_term_time; // Termination time of the call (used mainly in HANDOFF_RECV)
	int *dummy;
} event_content_type;

#define CROSS_PATH_GAIN		0.00000000000005
#define PATH_GAIN		0.0000000001
#define MIN_POWER		3
#define MAX_POWER		3000
#define SIR_AIM			10

// Taglia di 16 byte
typedef struct _sir_data_per_cell{
    double fading; // Fading of the call
    double power; // Power allocated to the call
} sir_data_per_cell;

// Taglia di 16 byte
typedef struct _channel{
	int channel_id; // Number of the channel
	sir_data_per_cell *sir_data; // Signal/Interference Ratio data
	struct _channel *next;
	struct _channel *prev;
} channel;


typedef struct _lp_state_type{
	int ecs_count;

	unsigned int channel_counter; // How many channels are currently free
	unsigned int arriving_calls; // How many calls have been delivered within this cell
	unsigned int complete_calls; // Number of calls which were completed within this cell
	unsigned int blocked_on_setup; // Number of calls blocked due to lack of free channels
	unsigned int blocked_on_handoff; // Number of calls blocked due to lack of free channels in HANDOFF_RECV
	unsigned int leaving_handoffs; // How many calls were diverted to a different cell
	unsigned int arriving_handoffs; // How many calls were received from other cells
	unsigned int cont_no_sir_aim; // Used for fading recheck
	unsigned int executed_events; // Total number of events

	simtime_t lvt; // Last executed event was at this simulation time

	double ta; // Current call interarrival frequency for this cell
	double ref_ta; // Initial call interarrival frequency (same for all cells)
	double ta_duration; // Average duration of a call
	double ta_change; // Average time after which a call is diverted to another cell

	int channels_per_cell; // Total channels in this cell
	int total_calls;

	bool check_fading; // Is the model set up to periodically recompute the fading of all ongoing calls?
	bool fading_recheck;
	bool variable_ta; // Should the call interarrival frequency change depending on the current time?

	unsigned int *channel_state;
	struct _channel *channels;
	int dummy;
	bool dummy_flag;
} lp_state_type;


double recompute_ta(double ref_ta, simtime_t now);
double generate_cross_path_gain(void);
double generate_path_gain(void);
void deallocation(unsigned int lp, lp_state_type *state, int channel, simtime_t);
int allocation(lp_state_type *state);
void fading_recheck(lp_state_type *pointer);


extern int channels_per_cell;


