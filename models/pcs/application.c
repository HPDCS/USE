#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ROOT-Sim.h>

#include "application.h"
#include <sys/mman.h>

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

  unsigned int *hot_spot_ids;

  double change_hot_spot_rate;
  double last_change_hot_spot;

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
	.change_hot_spot_rate = 2000.0,
	.last_change_hot_spot = 0.0,
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
    	if(args.enable_hot){
    		args.num_hot = NUM_HOT_CELLS;
    		args.ta_hot = TA_HOT;
    		args.channels_hot = CHANNELS_PER_HOT_CELL;
    		args.hot_spot_ids = mmap(NULL, n_prc_tot*sizeof(char), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    		for(i=0;i<args.num_hot;i++){
    			args.hot_spot_ids[i] = 1;
    		}
	    }
    	break;


	}
	return 0;
}


void build_unbalance(){
	unsigned int i,j=0;

	for(i=0;i<n_prc_tot;i++){
		if(GetNumaNode(i) == 0){
			args.hot_spot_ids[i] = 1;
			j++;
			if(j >= args.num_hot) break;
		}
	}
}


void ProcessEvent(unsigned int me, simtime_t now, int event_type, event_content_type *event_content, unsigned int size, void *ptr) {
	unsigned int w;
	(void)size;
	//printf("%d executing %d at %f\n", me, event_type, now);

	event_content_type new_event_content;

	new_event_content.cell = -1;
	new_event_content.channel = -1;
	new_event_content.call_term_time = -1;

	simtime_t handoff_time;
	simtime_t timestamp = 0;

	lp_state_type *state;
	state = (lp_state_type*)ptr;

	if(state != NULL) {
		state->lvt = now;
		state->executed_events++;

		if(args.enable_hot){
			double cold = state->ta;
			if(args.hot_spot_ids[me])
				state->ta = args.ta_hot;
			else
				state->ta = args.ta;
			if(cold != state->ta) printf("%u: change state!\n", me);
		}
	}


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

			state->ta_duration = args.ta_duration;
			state->ta_change = args.ta_change;
			state->ref_ta = state->ta = args.ta;
			state->channels_per_cell = args.channels;
			state->channel_counter = state->channels_per_cell;
			state->fading_recheck = args.fading_recheck;
			state->variable_ta    = args.variable_ta;
			state->fading_recheck_time = args.fading_recheck_time;


			
			if(args.enable_hot){
				state->channels_per_cell = args.channels_hot;
				state->rounds = 0;
				if(args.hot_spot_ids[me])
					state->ta = args.ta_hot;
			}



			// Show current configuration, only once
			if(me == 0) {
				printf("CURRENT CONFIGURATION:\n");
				printf("ncomplete calls: %d\n", args.total_calls);
				printf("ta: %f\n", args.ta);
				printf("ta_duration: %f\n", state->ta_duration);
				printf("ta_change: %f\n", state->ta_change); 
				printf("channels_per_cell: %d\n", args.channels);
				printf("expected occ. channels: %f\n", state->ta_duration/args.ta);
				printf("fading_recheck: %d\n", args.fading_recheck); 
				printf("variable_ta: %d\n", state->variable_ta);
				if(args.enable_hot){
					printf("hotspot config:\n");
					printf("   |- #partitions: %d\n", 		PARTITIONS);
					printf("   |- percentage of hot cells within the simulation model: %f\n", PERC_HOT);
					printf("   |- #cells per partition: %d\n", CELLS_PER_PARTITION);
					printf("   |- target skew: %f\n", 		TARGET_SKEW);
					printf("   |- slim partition:\n");
					printf("      |- #used channels: %f\n", MIN_LOAD_PARTITION);
					printf("   |- fat partition:\n");
					printf("      |- #used channels: %f\n", MAX_LOAD_PARTITION);
					printf("      |- #hot  cells (fat partition): %d\n", args.num_hot);
					printf("      |- #used channels from hot  cells: %f\n", LOAD_FROM_HOT_CELLS);
					printf("      |- #cold cells (fat partition): %d\n", NUM_CLD_CELLS_IN_MAX);
					printf("      |- #used channels from cold cells: %f\n", LOAD_FROM_CLD_CELLS);
					printf("      |- #channel per hot cell :   %d\n", 	args.channels_hot);
					printf("      |- ta_hot         : %f\n", args.ta_hot);
			  }
			}

			state->channel_counter = state->channels_per_cell;

			// Setup channel state
			state->channel_state = malloc(sizeof(unsigned int) * 2 * (state->channels_per_cell / BITS + 1));
			for (w = 0; w < state->channel_counter / (sizeof(int) * 8) + 1; w++)
				state->channel_state[w] = 0;

			// Start the simulation
			timestamp = (simtime_t) (20 * Random());
			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			// If needed, start the first fading recheck
			if (state->fading_recheck) {
				timestamp = (simtime_t) (state->fading_recheck_time * Random());
				ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
			}

			break;


		case START_CALL:

			state->arriving_calls++;

			if (state->channel_counter == 0) {
				state->blocked_on_setup++;
			} else {

				state->channel_counter--;

				new_event_content.channel = allocation(state);
				new_event_content.from = me;
				new_event_content.sent_at = now;

//				printf("(%d) allocation %d at %f\n", me, new_event_content.channel, now);

				// Determine call duration
				switch (args.duration_d) {

					case UNIFORM:
						new_event_content.call_term_time = now + (simtime_t)(state->ta_duration * Random());
						break;

					case EXPONENTIAL:
						new_event_content.call_term_time = now + (simtime_t)(Expent(state->ta_duration));
						break;

					default:
 						new_event_content.call_term_time = now + (simtime_t) (5 * Random() );
				}

				// Determine whether the call will be handed-off or not
				switch (args.handoff_d) {

					case UNIFORM:

						handoff_time  = now + (simtime_t)((state->ta_change) * Random());
						break;

					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent(state->ta_change));
						break;

					default:
						handoff_time = now + (simtime_t)(5 * Random());

				}

				if(new_event_content.call_term_time <= handoff_time+HANDOFF_SHIFT) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(TOPOLOGY_HEXAGON);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
				}
			}


			if (state->variable_ta)
				state->ta = recompute_ta(state->ref_ta, now);

			// Determine the time at which a new call will be issued
			switch (args.arrival_d) {

				case UNIFORM:
					timestamp= now + (simtime_t)(state->ta * Random());
					break;

				case EXPONENTIAL:
					timestamp= now + (simtime_t)(Expent(state->ta));
					break;

				default:
					timestamp= now + (simtime_t) (5 * Random());

			}

			ScheduleNewEvent(me, timestamp, START_CALL, NULL, 0);

			break;

		case END_CALL:

			state->channel_counter++;
			state->complete_calls++;
			deallocation(me, state, event_content->channel, now);

			break;

		case HANDOFF_LEAVE:

			state->channel_counter++;
			state->leaving_handoffs++;
			deallocation(me, state, event_content->channel, now);

			new_event_content.call_term_time =  event_content->call_term_time;
			new_event_content.from = me;
			new_event_content.dummy = &(state->dummy);
			ScheduleNewEvent(event_content->cell, now+HANDOFF_SHIFT, HANDOFF_RECV, &new_event_content, sizeof(new_event_content));
			break;

		case HANDOFF_RECV:
			state->arriving_handoffs++;
			state->arriving_calls++;

			if (state->channel_counter == 0)
				state->blocked_on_handoff++;
			else {
				state->channel_counter--;

				new_event_content.channel = allocation(state);
				new_event_content.call_term_time = event_content->call_term_time;


				switch (args.handoff_d) {
					case UNIFORM:
						handoff_time  = now + (simtime_t)((state->ta_change) * Random());
						break;
					case EXPONENTIAL:
						handoff_time = now + (simtime_t)(Expent( state->ta_change ));
						break;
					default:
						handoff_time = now+
						(simtime_t) (5 * Random());
				}

				if(new_event_content.call_term_time <= handoff_time+HANDOFF_SHIFT ) {
					ScheduleNewEvent(me, new_event_content.call_term_time, END_CALL, &new_event_content, sizeof(new_event_content));
				} else {
					new_event_content.cell = FindReceiver(TOPOLOGY_HEXAGON);
					ScheduleNewEvent(me, handoff_time, HANDOFF_LEAVE, &new_event_content, sizeof(new_event_content));
				}
			}


			break;


				case FADING_RECHECK:
						fading_recheck(state);
						timestamp = now + state->fading_recheck_time;
						ScheduleNewEvent(me, timestamp, FADING_RECHECK, NULL, 0);
			break;


		default:
			fprintf(stdout, "PCS: Unknown event type! (me = %d - event type = %d)\n", me, event_type);
			abort();

	}
}


bool OnGVT(unsigned int me, lp_state_type *snapshot) {
	(void)me;
	if(me == 0 && args.enable_hot && args.enable_moving_hot && (snapshot->lvt - args.last_change_hot_spot) > args.change_hot_spot_rate && (snapshot->rounds % 2 == 0) ){
		printf("%u: ITS time to get a change at %f\n", me, snapshot->lvt);

		for(unsigned int i = 0; i<n_prc_tot;i++){
			args.hot_spot_ids[i] = 0;
		}

		build_unbalance();
		args.last_change_hot_spot = snapshot->lvt;

	} else {
		snapshot->rounds++;
	}

	if (snapshot->complete_calls < args.total_calls)
		return false;
	return true;
}
