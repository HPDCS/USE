
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>


#include "application.h"
#include "setup.h"

double recompute_ta(double, simtime_t);
double generate_cross_path_gain(uint32_t *, uint32_t *);
double generate_path_gain(uint32_t *, uint32_t *);
void deallocation(unsigned int, lp_state_type *, int, simtime_t);
int allocation(lp_state_type *, uint32_t *, uint32_t *);


//lp_state_type* states[OBJECTS];
//#define state states[me]

__thread char buff1[64]; 
__thread char buff2[64]; 

#ifdef NUMA_UBIQUITOUS
#define displacement (1<<21)
#define SET_MEMORY(addr, value) \
    do { \
        *(addr) = (value); \
	for (int _i = 1; _i < (MEM_NODES); ++_i){ \
        	*((typeof(addr))((char*)(addr) + _i * (displacement))) = *(addr); \
	} \
    } while(0)
#else
#define SET_MEMORY(addr,value) *(addr) = (value)
#endif

bool pcs_statistics = false;
unsigned int complete_calls = COMPLETE_CALLS;

//#define UNBALANCE
#define DUMMY_TA 500

double ran;




typedef struct model_parameters{
	simtime_t ta;
	simtime_t ta_duration;
	simtime_t ta_change;
	simtime_t fading_recheck_time;

	unsigned int channels; 
	unsigned int total_calls;

	simtime_t ta_hot;
	unsigned int channels_hot; 
	unsigned int num_hot;

 char *hot_spot_ids;

  double change_hot_spot_rate;
  double last_change_hot_spot;
  int rounds;

	bool check_fading; 
	bool fading_recheck;
	bool variable_ta; 
	bool enable_hot;

	bool enable_moving_hot;
	unsigned char arrival_d; 
	unsigned char duration_d; 
	unsigned char handoff_d; 
}
model_parameters;

struct argp_option model_options[] = {
  {"ta",                  1000, "TIME", 0, "Interarrival Time"               , 0 },
  {"hot-ta",              1001, "TIME", 0, "Hot-interarrival Time"               , 0 },
  {"duration",            1002, "TIME", 0, "Call duration"               , 0 },
  {"handoff-rate",        1003, "TIME", 0, "Call handoff rate"               , 0 },
  {"ch",                  1004, "VALUE", 0, "Number of channels per cell"               , 0 },
  {"hot-ch",              1005, "VALUE", 0, "Number of channels per hot cell"               , 0 },
  {"enable-fading",       1006, 0, 0, "Enable fading recheck"               , 0 },
  {"enable-variable-ta",  1007, 0, 0, "Enable variable interarrival time"               , 0 },
  {"num-calls",           1008, "VALUE", 0, "Number of calls per cell to end the simulation"               , 0 },
  {"fading_time",         1009, 0, 0, "Fading time"               , 0 },
  {"arrival-d",           1010, "VALUE", 0, "0=Uniform 1=Exponential (default)"               , 0 },
  {"duration-d",          1011, "VALUE", 0, "0=Uniform 1=Exponential (default)"               , 0 },
  {"handoff-d",           1012, "VALUE", 0, "0=Uniform 1=Exponential (default)"               , 0 },
  {"enable-hot",          1013, 0, 0, "Enable hot cells"               , 0 },
  {"enable-dyn-hot",      1014, 0, 0, "Enable dyn hot cells"               , 0 },
  { 0, 0, 0, 0, 0, 0} 
};

model_parameters args = {
	.ta = 0.4,
	.ta_hot = 0,
	.ta_duration = 120,
	.ta_change = 300,
	.channels = 1000,
	.variable_ta = 0,
	.fading_recheck = 0,
	.fading_recheck_time = 300,
	.total_calls = 0,
	.arrival_d = 1, 
	.duration_d = 1, 
	.handoff_d = 1, 
	.enable_hot = 0,
	.enable_moving_hot = 0,
	.num_hot = 0,
	.hot_spot_ids = NULL,
	.change_hot_spot_rate = 1500.0,
	.last_change_hot_spot = 0.0,
	.rounds = 0,
};

error_t model_parse_opt(int key, char *arg, struct argp_state *state){
	unsigned int i;
	(void)state;
	switch(key){
		case 1000:
			args.ta = strtod(arg, NULL);
			break;
		case 1001:
			args.ta_hot = strtod(arg, NULL);
			break;
		case 1002:
			args.ta_duration = strtod(arg, NULL);
			break;
		case 1003:
			args.ta_change = strtod(arg, NULL);
			break;
		case 1004:
			args.channels = atoi(arg);
			break;
		case 1005:
			args.channels_hot = atoi(arg);
			break;
		case 1006:
			args.fading_recheck = 1;
			break;
		case 1007:
			args.variable_ta = 1;
			break;
		case 1008:
			args.total_calls = atoi(arg);
			break;
		case 1009:
			args.fading_recheck_time = atoi(arg);
			break;
		case 1010:
			args.arrival_d = atoi(arg);
			break;
		case 1011:
			args.duration_d = atoi(arg);
			break;
		case 1012:
			args.handoff_d = atoi(arg);
			break;
		case 1013:
			args.enable_hot = 1;
			break;
		case 1014:
			args.enable_moving_hot = 1;
			break;
    case ARGP_KEY_END:
    	break;


	}
	return 0;
}


//callback function for processing an event at an object
void ProcessEvent(unsigned int me, double now, int event_type, void *the_event_content, unsigned int size, void *ptr) {

	unsigned int w;
	unsigned int temp;
        uint32_t *s1, *s2;
	int init_calls = INITIAL_CALLS;
	event_content_type *event_content;
	event_content_type new_event_content;

	lp_state_type *state;
	state = (lp_state_type*)ptr;

	//bypassing compiler warnings
	ptr = ptr;
	size = size;

	event_content = (event_content_type*)the_event_content;

	new_event_content.cell = -1;
	new_event_content.channel = -1;
	new_event_content.call_term_time = -1;

	simtime_t handoff_time;
	simtime_t timestamp = 0;

	int i;
	int new_call = 1;

	double barrier = (double)((int)(now / LOOKAHEAD) * LOOKAHEAD) + LOOKAHEAD + 0.00000001;

	if(state != NULL) {
		SET_MEMORY(&state->lvt, now);
		temp = state->executed_events + 1;
		SET_MEMORY(&state->executed_events, temp);

		s1 = &(state->seed1);
        	s2 = &(state->seed2);
	}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

	switch(event_type) {

		case INIT:

			// Initialize the LP's state
			state = (lp_state_type *)malloc(sizeof(lp_state_type));
			if (state == NULL){
				printf("Out of memory!\n");
				exit(EXIT_FAILURE);
			}

			SetState(state);
			bzero(state, sizeof(lp_state_type));

			s1 = &(state->seed1);
                        s2 = &(state->seed2);
                        *s1 = (0x01 * me) + 1;
                        *s2 = *s1 ^ *s1;

			state->cell_id = me;

			state->channel_counter = CHANNELS_PER_CELL;

#ifdef UNBALANCE
			state->ref_ta = state->ta = TA;
			if(me < (OBJECTS >> 1)) state->ref_ta = state->ta = TA / 2;
#else
			state->ref_ta = state->ta = TA;
#endif
			init_calls = (int)((double)TA_DURATION / (double)state->ta);

			state->ta_duration = TA_DURATION;
			state->ta_change = TA_CHANGE;
			state->channels_per_cell = CHANNELS_PER_CELL;

			// Show current configuration, just once
			if(me == 0) {
				printf("CURRENT CONFIGURATION:\ncomplete calls: %d\nTA: %f\nta_duration: %f\nta_change: %f\nchannels_per_cell: %d\nfading_recheck: %d\nvariable_ta: %d\nseeds: %d-%d\nInitial calls: %d\n",
					complete_calls, state->ta, state->ta_duration, state->ta_change, state->channels_per_cell, state->fading_recheck, state->variable_ta,state->seed1,state->seed2,init_calls);
				fflush(stdout);
			}


			state->channel_counter = state->channels_per_cell;

			// Setup channel state
			state->channel_state = malloc(sizeof(unsigned int) * 2 * (CHANNELS_PER_CELL / BITS + 1));
			for (w = 0; w < state->channel_counter / (sizeof(int) * 8) + 1; w++)
				state->channel_state[w] = 0;

			// Start the simulation
			for(i = 0 ; i < init_calls; i++){
				timestamp = (simtime_t) (0.2 * Random(s1,s2));
				ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);
				//printf("INIT: scheduled new START_CALL at time %e\n",timestamp);
			}


#ifdef NUMA_BALANCING 
#ifdef NUMA_UBIQUITOUS
                        memcpy((char*)state+(1<<21),(char*)state,sizeof(lp_state_type));
#endif
#endif
			break;

		case HANDOFF_IN:

			init_calls = (int)((double)TA_DURATION / (double)state->ta);
			new_call = 0;

		case START_CALL:

			init_calls = (int)((double)TA_DURATION / (double)state->ta);

			temp = state->arriving_calls + 1;
			SET_MEMORY(&state->arriving_calls, temp);

			if (state->channel_counter == 0) {
				temp = state->blocked_on_setup + 1;
				SET_MEMORY(&state->blocked_on_setup, temp);
				printf("no channel available!!\n");
			} else {
				temp = state->channel_counter - 1;
				SET_MEMORY(&state->channel_counter, temp);

				new_event_content.channel = allocation(state,s1,s2);
				new_event_content.from = me;
				new_event_content.sent_at = now;

				// Determine call duration
				switch (DURATION_DISTRIBUTION) {

					case UNIFORM:
						new_event_content.call_term_time = now + (simtime_t)(state->ta_duration * Random(s1,s2));
						break;

					case EXPONENTIAL:
						new_event_content.call_term_time = now + (simtime_t)(Expent(state->ta_duration,s1,s2));
						break;

					default:
 						new_event_content.call_term_time = now + (simtime_t) (5 * Random(s1,s2) );
				}

//				printf("OBJ %d - call termination time is %e\n",me,new_event_content.call_term_time);

				// Determine whether the call will be handed-off or not
				switch (CELL_CHANGE_DISTRIBUTION) {

					case UNIFORM:
						handoff_time  = now + (simtime_t)((state->ta_change) * Random(s1,s2));
						break;

					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent(state->ta_change,s1,s2));
						break;

					default:
						handoff_time = now + (simtime_t)(5 * Random(s1,s2));

				}

				if(new_event_content.call_term_time < barrier) new_event_content.call_term_time = barrier;
				if(handoff_time < barrier) handoff_time = barrier;

				if( new_event_content.call_term_time <= handoff_time+HANDOFF_SHIFT) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, (char*)&new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(me,TOPOLOGY_HEXAGON,s1,s2);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, (char*)&new_event_content, sizeof(new_event_content));
					ScheduleNewEvent(new_event_content.cell, handoff_time+HANDOFF_SHIFT, HANDOFF_IN, (char*)&new_event_content, sizeof(new_event_content));
				}
			}

			if (new_call == 1){
				// Determine the time at which a new call will be issued
				switch (DISTRIBUTION) {
	
					case UNIFORM:
						timestamp= now + init_calls * (simtime_t)(state->ta * Random(s1,s2));

						break;
	
					case EXPONENTIAL:
						timestamp= now + init_calls * (simtime_t)(Expent(state->ta,s1,s2));
						break;
	
					default:
						timestamp= now + INITIAL_CALLS * (simtime_t) (5 * Random(s1,s2));
	
				}
	
				if (timestamp < (now + LOOKAHEAD)) timestamp = now + LOOKAHEAD;
	
				ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);
	
			}	
				break;

		case END_CALL:

			temp = state->channel_counter + 1;
			SET_MEMORY(&state->channel_counter, temp);
			temp = state->complete_calls + 1;
			SET_MEMORY(&state->complete_calls, temp);
			deallocation(me, state, event_content->channel, now);

			break;

		case HANDOFF_LEAVE:

			temp = state->channel_counter + 1;
			SET_MEMORY(&state->channel_counter, temp);

			temp = state->leaving_handoffs + 1;
			SET_MEMORY(&state->leaving_handoffs, temp);

			deallocation(me, state, event_content->channel, now);

			break;

		case HANDOFF_RECV:
			temp = state->arriving_handoffs + 1;
			SET_MEMORY(&state->arriving_handoffs, temp);

			temp = state->arriving_calls + 1;
			SET_MEMORY(&state->arriving_calls, temp);

			if (state->channel_counter == 0){
				temp = state->blocked_on_handoff + 1;
				SET_MEMORY(&state->blocked_on_handoff, temp);
			} else {
				temp = state->channel_counter - 1;
				SET_MEMORY(&state->channel_counter, temp);

				new_event_content.channel = allocation(state,s1,s2);
				new_event_content.call_term_time = event_content->call_term_time;

				switch (CELL_CHANGE_DISTRIBUTION) {
					case UNIFORM:
						handoff_time  = now + (simtime_t)((state->ta_change) * Random(s1,s2));

						break;
					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent( state->ta_change ,s1,s2));

						break;
					default:
						handoff_time = now+
						(simtime_t) (5 * Random(s1,s2));
				}

				if( new_event_content.call_term_time <= handoff_time+HANDOFF_SHIFT ) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, (char*)&new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(me,TOPOLOGY_HEXAGON,s1,s2);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, (char*)&new_event_content, sizeof(new_event_content));
				}
			}

			break;

		default:
			fprintf(stdout, "PCS: Unknown event type! (me = %d - event type = %d)\n", me, event_type);
			abort();

	}

#pragma GCC diagnostic pop

	SET_MEMORY(s1,*s1);
        SET_MEMORY(s2,*s2);
}


#define HOUR                    3600
#define DAY                             (24 * HOUR)
#define WEEK                    (7 * DAY)

#define EARLY_MORNING   8.5 * HOUR
#define MORNING                 13 * HOUR
#define LUNCH                   15 * HOUR
#define AFTERNOON               19 * HOUR
#define EVENING                 21 * HOUR


#define EARLY_MORNING_FACTOR    4
#define MORNING_FACTOR          0.8
#define LUNCH_FACTOR            2.5
#define AFTERNOON_FACTOR        2
#define EVENING_FACTOR          2.2
#define NIGHT_FACTOR            4.5
#define WEEKEND_FACTOR          5


bool OnGVT(unsigned int me, lp_state_type *snapshot) { return false; }

double recompute_ta(double ref_ta, simtime_t time_now) {

        int now = (int)time_now;
        now %= WEEK;

        if (now > 5 * DAY)
                return ref_ta * WEEKEND_FACTOR;

        now %= DAY;

        if (now < EARLY_MORNING)
                return ref_ta * EARLY_MORNING_FACTOR;
        if (now < MORNING)
                return ref_ta * MORNING_FACTOR;
        if (now < LUNCH)
                return ref_ta * LUNCH_FACTOR;
        if (now < AFTERNOON)
                return ref_ta * AFTERNOON_FACTOR;
        if (now < EVENING)
                return ref_ta * EVENING_FACTOR;

        return ref_ta * NIGHT_FACTOR;
}

inline double my_pow(double x, int y) {
    double result = 1.0;
    for (int i = 0; i < y; ++i) {
        result *= x;
    }
    return result;
}

double generate_cross_path_gain(uint32_t * s1, uint32_t * s2) {
        double value;
        double variation;

        variation = 10 * Random(s1,s2);
        variation = my_pow ((double)10.0 , (int)(variation / 10));
        value = CROSS_PATH_GAIN * variation;
        return (value);
}

double generate_path_gain(uint32_t * s1, uint32_t * s2) {
        double value;
        double variation;

        variation = 10 * Random(s1,s2);
        variation = pow ((double)10.0 , (variation / 10));
        value = PATH_GAIN * variation;
        return (value);
}

void deallocation(unsigned int me, lp_state_type *pointer, int ch, simtime_t lvt) {
        channel *c;

        c = pointer->channels;
        while(c != NULL){
                if(c->channel_id == ch)
                        break;
                c = c->prev;
        }
        if(c != NULL){
                if(c == pointer->channels){
                        SET_MEMORY(&pointer->channels, c->prev);
                        if(pointer->channels)
                                SET_MEMORY(&pointer->channels->next, NULL);
                }
                else{
                        if(c->next != NULL)
                                SET_MEMORY(&c->next->prev, c->prev);
                        if(c->prev != NULL)
                                SET_MEMORY(&c->prev->next, c->next);
                }
                RESET_CHANNEL(pointer, ch);
                free(c->sir_data);

                free(c);
        } else {
                printf("(%d) Unable to deallocate on %p, channel is %d at time %f\n", me, c, ch, lvt);
                abort();
        }
        return;
}

void fading_recheck(lp_state_type *pointer, uint32_t * s1, uint32_t * s2) {
        channel *ch;

        ch = pointer->channels;

        while(ch != NULL){
                ch->sir_data->fading = Expent(1.0,s1,s2);
                SET_MEMORY(&ch->sir_data->fading, ch->sir_data->fading);
                ch = ch->prev;
        }
}

int allocation(lp_state_type *pointer, uint32_t * s1, uint32_t * s2) {

        int i;
        int index;
        double summ;
        channel *c, *ch;

        index = -1;
        for(i = 0; i < pointer->channels_per_cell; i++){
                if(!CHECK_CHANNEL(pointer,i)){
                        index = i;
                        break;
                }
        }

        if(index != -1){

                SET_CHANNEL(pointer,index);

                c = (channel*)malloc(sizeof(channel));
                if(c == NULL){
                        printf("malloc error: unable to allocate channel!\n");
                        exit(-1);
                }

                SET_MEMORY(&c->next,NULL);
                SET_MEMORY(&c->prev, pointer->channels);
                SET_MEMORY(&c->channel_id, index);      
                c->sir_data = (sir_data_per_cell*)malloc(sizeof(sir_data_per_cell));
                SET_MEMORY(&c->sir_data, c->sir_data);
                if(c->sir_data == NULL){
                        printf("malloc error: unable to allocate SIR data!\n");
                        exit(-1);
                }


                if(pointer->channels != NULL)
                        SET_MEMORY(&pointer->channels->next,c);
                SET_MEMORY(&pointer->channels, c);

                summ = 0.0;

                ch = pointer->channels->prev;
                while(ch != NULL){
                        summ += generate_cross_path_gain(s1,s2) *  ch->sir_data->power * ch->sir_data->fading ;
                        ch = ch->prev;
                }

                if (summ == 0.0) {
                        // The newly allocated channel receives the minimal power
                        SET_MEMORY(&c->sir_data->power, MIN_POWER);
                } else {
                        c->sir_data->fading = Expent(1.0,s1,s2);
                        SET_MEMORY(&c->sir_data->fading, c->sir_data->fading);
                        c->sir_data->power = ((SIR_AIM * summ) / (generate_path_gain(s1,s2) * c->sir_data->fading));
                        SET_MEMORY(&c->sir_data->power, c->sir_data->power);
                        if (c->sir_data->power < MIN_POWER)
                                //c->sir_data->power = MIN_POWER;
                                SET_MEMORY(&c->sir_data->power, MIN_POWER);
                        if (c->sir_data->power > MAX_POWER)
                                //c->sir_data->power = MAX_POWER;
                                SET_MEMORY(&c->sir_data->power, MAX_POWER);
                }

        } else {
                printf("Unable to allocate channel, but the counter says I have %d available channels\n", pointer->channel_counter);
                abort();
                fflush(stdout);
        }

        return index;
}



