#include <ROOT-Sim.h>



/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORM			0
#define EXPONENTIAL		1
#define DISTRIBUTION	1

#define COMPLETE_CALLS		5000

#ifndef TA
#define TA		0.9
#endif

#ifndef TA_DURATION
#define TA_DURATION		120
#endif

#ifndef CHANNELS_PER_CELL
#define CHANNELS_PER_CELL	1000
#endif

#ifndef TA_CHANGE
#define TA_CHANGE		50.0
#endif

#ifndef TA_HOTNESS
#define TA_HOTNESS		500.0
#endif

#define	HOTNESS_CHANGE_DISTRIBUTION	EXPONENTIAL
#define	CELL_CHANGE_DISTRIBUTION	EXPONENTIAL
#define DURATION_DISTRIBUTION		EXPONENTIAL

/* Cell's hotness parameters */
#ifndef HOT_CELL_FACTOR
#define HOT_CELL_FACTOR				1024
#endif

#ifndef START_CALL_HOT_FACTOR
#define START_CALL_HOT_FACTOR		1.0
#endif

#ifndef CALL_DURATION_HOT_FACTOR
#define CALL_DURATION_HOT_FACTOR	1.0
#endif

#ifndef HANDOFF_LEAVE_HOT_FACTOR
#define HANDOFF_LEAVE_HOT_FACTOR	1.0
#endif

/* Channel states */
#define CHAN_BUSY	1
#define CHAN_FREE	0

/* EVENT TYPES - PCS */
#define START_CALL		 1
#define END_CALL		 2
#define HANDOFF_LEAVE	 3
#define HANDOFF_RECV	 4
#define FADING_RECHECK	 5
#define HOTNESS_LEAVE	 6
#define HOTNESS_RECV	 7

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
	unsigned int hot_cell; // Number of hot tokens maintained

	simtime_t lvt; // Last executed event was at this simulation time

	double ta; // Current call interarrival frequency for this cell
	double ref_ta; // Initial call interarrival frequency (same for all cells)
	double ta_duration; // Average duration of a call
	double ta_change; // Average time after which a call is diverted to another cell
	double ta_hotness; // Average time after which hotness is given to another cell

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

