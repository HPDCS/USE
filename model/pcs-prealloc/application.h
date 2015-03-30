#include <ROOT-Sim.h>







// per baseline incrementale
//#define INCREMENTAL_BASELINE



#ifdef INCREMENTAL_BASELINE
	#define make_copy(dest, value) do {\
					dirty_mem(&(dest), sizeof(long long));\
					dest = value;\
				} while(0)
#else
	#define make_copy(dest, value)  dest = value
#endif






/* DISTRIBUZIONI TIMESTAMP */
#define UNIFORME	0
#define ESPONENZIALE 	1

#define DISTRIBUZIONE 1


#define ACCURATE_SIMULATION

#define PRE_SCHEDULING

#ifndef COMPLETE_CALLS
	#define COMPLETE_CALLS 1000
#endif

#ifndef TA
	#define TA 3.6
#endif

#ifndef TA_DURATA
	#define TA_DURATA 120
#endif

#ifndef CHANNELS_PER_CELL
	#define   CHANNELS_PER_CELL    100
#endif


#ifndef TA_CAMBIO
#define TA_CAMBIO 300.0
#endif
#define	DISTRIBUZIONE_CAMBIOCELLA ESPONENZIALE
#define DISTRIBUZIONE_DURATA ESPONENZIALE

#define TOPOLOGY EXAGON
//#define TOPOLOGY BID_RING 
#define D  (int)sqrt(n_prc_tot)

/* TOPOLOGIE */
#define POINT_TO_POINT 		0		/* punto- punto*/
#define BID_RING		1		/* anello bidirezionale */
#define UNI_RING		2		/* anello unidirezione */
#define HYPER			3		/* ipercubo */
#define STAR			4
#define MESH			5
#define EXAGON                  6

/* Channel states */
#define CHAN_BUSY 1
#define CHAN_FREE 0

/* EVENT TYPES - PCS */
#define INIT		 0
#define START_CALL	21
#define END_CALL        17
#define HANDOFF_LEAVE   18
#define HANDOFF_RECV    20
#define FADING_RECHECK	24

#define FADING_RECHECK_FREQUENCY	3600	// Each 5 Minutes

#define MSK 0x1



#define SET_CHANNEL_BIT(B,K) make_copy(B, B | (MSK << K))
#define RESET_CHANNEL_BIT(B,K) make_copy(B, B & ~(MSK << K))
#define CHECK_CHANNEL_BIT(B,K) ( B & (MSK << K) )

#define BITS (sizeof(int) * 8)

#define CHECK_CHANNEL(P,I) CHECK_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) 
#define SET_CHANNEL(P,I)  SET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) 
#define RESET_CHANNEL(P,I) RESET_CHANNEL_BIT(						\
	((unsigned int*)(((lp_state_type*)P)->channel_state))[(int)((int)I / BITS)],	\
	((int)I % BITS)) 


// struttura rappresentante il contenuto applicativo di un messaggio scambiato tra LP
typedef struct _event_content_type {
	unsigned int cell;
	unsigned int second_cell;
	int channel;
	simtime_t   call_term_time;
} event_content_type;

// Taglia di 28 byte
typedef struct __PCS_routing{
	int num_adjacent;
	unsigned int adjacent_identities[6];
} _PCS_routing;


#define CROSS_PATH_GAIN (0.00000000000005)
#define PATH_GAIN (0.0000000001)
#define MIN_POWER (3)
#define MAX_POWER (3000)
#define SIR_AIM		(10)

// Taglia di 16 byte
typedef struct _sir_data_per_cell{

    //double distance;
    double fading;
    double power;
} sir_data_per_cell;

// Taglia di 16 byte
typedef struct _channel{
	int channel_id;
	int used;
	sir_data_per_cell sir_data;
} channel;


// Taglia di 68 byte + 4*dim array
typedef struct _lp_state_type{
	simtime_t time;
	bool check_fading;
	unsigned int contatore_canali;
	unsigned int cont_chiamate_entranti;
	unsigned int cont_chiamate_complete;
	unsigned int cont_bloccate_in_partenza;
	unsigned int cont_bloccate_in_handoff;
	unsigned int cont_handoff_uscita;
	unsigned int handoffs_entranti;
	unsigned int cont_no_sir_aim;
	unsigned long contatore_eventi;
//	simtime_t time;
	unsigned int channel_state[2 * (CHANNELS_PER_CELL / BITS) + 1];
	_PCS_routing *buff_topology;
	struct _channel channels[CHANNELS_PER_CELL];
} lp_state_type;


void set_topology();
void set_my_topology(unsigned int gid, _PCS_routing *buffer);
int __FindReceiver(unsigned int sender , void * addr);

double recompute_ta(double ref_ta, simtime_t now);
double generate_cross_path_gain(lp_state_type * pointer);
double generate_path_gain(lp_state_type * pointer);
void deallocation(unsigned int lp, lp_state_type * pointer, int channel);
int allocation(unsigned int me, lp_state_type * pointer);

extern int channels_per_cell;
