#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "application.h"

#define HOUR			3600
#define DAY			(24 * HOUR)
#define WEEK			(7 * DAY)

#define EARLY_MORNING		8.5 * HOUR
#define MORNING			13 * HOUR
#define LUNCH			15 * HOUR
#define AFTERNOON		19 * HOUR
#define EVENING			21 * HOUR


#define EARLY_MORNING_FACTOR	4
#define MORNING_FACTOR		0.8
#define LUNCH_FACTOR		2.5
#define AFTERNOON_FACTOR	2
#define EVENING_FACTOR		2.2
#define NIGHT_FACTOR		4.5
#define WEEKEND_FACTOR		5

extern int kid;
extern int channels_per_cell;
extern bool power_management;


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


double generate_cross_path_gain(lp_state_type *pointer) {
	double value;
	double variation;
	
	variation = 10 * Random();
	variation = pow ((double)10.0 , (variation / 10));
	value = CROSS_PATH_GAIN * variation;
	return (value);
}

double generate_path_gain(lp_state_type *pointer) {
	double value;
	double variation;
	
	variation = 10 * Random();
	variation = pow ((double)10.0 , (variation / 10));
	value = PATH_GAIN * variation;
	return (value);
}

void deallocation(unsigned int me, lp_state_type *pointer, int ch) {
	channel *c;

	c = &pointer->channels[ch];
	
	if(c->used == 1) {
//		if(c == pointer->channels){
//			pointer->channels = c->prev;
//			if(pointer->channels)
//				pointer->channels->next = NULL;
//		}
//		else{
//			if(c->next != NULL)
//				c->next->prev = c->prev;
//			if(c->prev != NULL)
//				c->prev->next = c->next;
//		}
		RESET_CHANNEL(pointer, ch);
//		if (power_management)
//			free(c->sir_data);
		
//		free(c);
		make_copy(c->sir_data.fading, 0);
		make_copy(c->sir_data.power, 0);
		make_copy(c->used, 0);
		//c->sir_data.fading = 0.;
		//c->sir_data.power = 0.;
		//c->used = 0;
	} else
		printf("(%d) Unable to deallocate on %d\n", me, ch);

}

int allocation(unsigned int me, lp_state_type *pointer) {
	
	int i;
  	int index;
  	int idx = -1;
	double summ;
	
	channel *c, *ch;

	for(i = 0; i < CHANNELS_PER_CELL; i++) {
		if(!CHECK_CHANNEL(pointer, i)){
			idx = i;
			break;
		}
	}
	
	if(idx != -1) {
		index = idx;
		
		SET_CHANNEL(pointer,index);

		c = &pointer->channels[index];
		//c->used = 1;
		make_copy(c->used, 1);
//		if(c == NULL){
//			printf("malloc error: unable to allocate channel!\n");
//			exit(-1);
//		}
	
//		c->next = NULL;
//		c->prev = pointer->channels;
		c->channel_id = index;
//		c->sir_data = (sir_data_per_cell*)malloc(sizeof(sir_data_per_cell));
//		if(c->sir_data == NULL){
//			printf("malloc error: unable to allocate SIR data!\n");
//			exit(-1);
//		}
		
//		if(pointer->channels != NULL)
//			pointer->channels->next = c;
//		pointer->channels = c;
	
		summ = 0.0;
		
//		if (pointer->contatore_canali < 0.65 * CHANNELS_PER_CELL) {
		if (power_management || pointer->check_fading) {
			ch = &pointer->channels[index--];

			while(index >= 0){ // TODO: scandire tutti quelli used == 1
				//ch->sir_data.fading = Expent(1.0);
				make_copy(c->sir_data.fading, Expent(1.0));
	
				summ += generate_cross_path_gain(pointer) * ch->sir_data.power * ch->sir_data.fading ;
				ch = &pointer->channels[index--];
			}
		}

		if (summ == 0.0) {
			// The newly allocated channel receives the minimal power
			//c->sir_data.power = MIN_POWER;
			make_copy(c->sir_data.power, MIN_POWER);
		} else {
			make_copy(c->sir_data.fading, Expent(1.0));
		  	//c->sir_data.fading = Expent(1.0);
			//c->sir_data.power = ((SIR_AIM * summ) / (generate_path_gain(pointer) * c->sir_data.fading));
			make_copy(c->sir_data.power, ((SIR_AIM * summ) / (generate_path_gain(pointer) * c->sir_data.fading)));
			if (c->sir_data.power < MIN_POWER) make_copy(c->sir_data.power, MIN_POWER);
			if (c->sir_data.power > MAX_POWER) make_copy(c->sir_data.power, MAX_POWER);
		}

	} else {
		printf("Unable to allocate channel, but the counter says I have %d available channels\n", pointer->contatore_canali);
		fflush(stdout);
	}
	
    return idx;
}

