#include "core.h"
#include "timer.h"
#include "scheduler.h"




__thread unsigned int fetch_mercy_period = 64;
__thread int fetch_mercy_period_step = 16; //se negativo, indica che lo sto riducendo
__thread unsigned long long curr_clock = 0;
__thread unsigned long long prev_clock = 0;
__thread double curr_throughput = 0;
__thread double prev_throughput = 0;
__thread unsigned int prev_committed = 0;
__thread unsigned int curr_committed = 0;
__thread unsigned int events_committed = 0;
__thread unsigned int mercy_period_recalc = 0;
__thread unsigned int skipped_events;
__thread simtime_t min_fetchable;


void scheduler_update_state(){
		if(mercy_period_recalc++ > 10000 /*mercy_period_recalc_period*/){
			//PERIODIC MERCY PERIOD RECALCULATION
			curr_clock = CLOCK_READ();
			curr_committed = events_committed;
			curr_throughput = ((double)(curr_committed - prev_committed)*1000000)/((double)(curr_clock - prev_clock));

		#if VERBOSE > 1
			if (tid == MAIN_PROCESS){ 
				printf("[%u] MERCY_STEP %d -> MERCY_PERIOD %d\n", tid, fetch_mercy_period_step, fetch_mercy_period);
				printf("[%u] \tPREV_COMM %u - PREV_THR %f\n", tid, prev_committed, prev_throughput);
				printf("[%u] \tCURR_COMM %u - CURR_THR %f\n", tid, curr_committed, curr_throughput);
			}
		#endif
			if(prev_throughput > 0){
				if (curr_throughput > prev_throughput){
					fetch_mercy_period_step = (fetch_mercy_period_step << 1) >> (fetch_mercy_period_step >= 32 || fetch_mercy_period_step <= -32);
					//if(same_direction++ > same_direction_threshold)
					//	mercy_period_recalc_period/2;
					//	same_direction = 0;
					//}
					//change_direction = 0;
				}
				else{
					fetch_mercy_period_step = -((fetch_mercy_period_step >> 1) << 1*(fetch_mercy_period_step <= 4 && fetch_mercy_period_step >= -4));
					//if(change_direction++ > change_direction_threshold){
					//	mercy_period_recalc_period*2;
					//	change_direction = 0;
					//}
					//same_direction = 0;
				}

				if(((int)fetch_mercy_period + fetch_mercy_period_step) > 0){
					fetch_mercy_period += fetch_mercy_period_step;
					if(fetch_mercy_period > 128) fetch_mercy_period = 128; 
				}
				else{
					fetch_mercy_period = 0; 
					fetch_mercy_period_step /= -1*fetch_mercy_period_step; 
				}
			}

			prev_throughput = curr_throughput;
			prev_clock = curr_clock;
			prev_committed = curr_committed;

			mercy_period_recalc = 0;
		}

}