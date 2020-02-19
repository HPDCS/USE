#include <lp_stats.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <core.h>
#include <simtypes.h>
#include <hpdcs_utils.h>
#include <handle_interrupt.h>

//static inline __attribute__((always_inline))
void init_lp_stats(LP_state ** LPS, unsigned int n_prc_tot)
{
	unsigned int index;
	if (LPS == NULL){
		printf("invalid LPS\n");
		gdb_abort;
		return;
	}
	for (index=0; index<n_prc_tot; index++)
	{
		if (LPS[index] == NULL){
			printf("LPS is null\n");
			gdb_abort;
		}

		//static allocation start,decomment struct definition
		if (LPS[index]->lp_statistics == NULL){
			if ( (LPS[index]->lp_statistics = malloc(sizeof(lp_evt_stats)) ) == NULL){
				printf("no memory available to allocate lp_stats\n");
				gdb_abort;
			}
		}
		memset((void *) LPS[index]->lp_statistics, NO_TIMER, sizeof(lp_evt_stats));
		//static allocation end
	}
}

//static inline __attribute__((always_inline))
void store_lp_stats(unsigned int lp_idx, unsigned int s, unsigned int t, clock_timer time)
{
	lp_evt_stats *lps=LPS[lp_idx]->lp_statistics;
	#if DEBUG==1
	if( (s!=LP_STATE_READY) && (s!=LP_STATE_ROLLBACK) ){
		printf("invalid LP_state\n");
		gdb_abort;
	}

	#endif

	s = (s != LP_STATE_READY) ? 1 : 0;

	if (lps->lp_state[s].evt_type[t].avg_exec_time == NO_TIMER)
	{
		lps->lp_state[s].evt_type[t].avg_exec_time = time;
		#if DEBUG==1
		if(lps->max_timer < time)
			lps->max_timer = time;
		#endif
	}
	else
	{
		#if DEBUG==1
		clock_timer old_max=lps->max_timer;
		if(lps->max_timer < time)
		{
			lps->max_timer=time;
		}
		clock_timer old_mean=lps->lp_state[s].evt_type[t].avg_exec_time;
		if(old_mean > lps->max_timer){
			printf("invalid old_mean\n");
			printf("max timer=%llu,old_max=%llu,old_mean=%llu,actual_time=%llu,s=%u,t=%u\n",
				(unsigned long long)lps->max_timer,(unsigned long long )old_max,
				(unsigned long long)old_mean,(unsigned long long)time,s,t);
			gdb_abort;
		}
		#endif

		//lps->lp_state[s].evt_type[t].avg_exec_time = (clock_timer) ((ALPHA * ((double) time)) + ((1.0 - ALPHA) * ((double) lps->lp_state[s].evt_type[t].avg_exec_time)));
		
		lps->lp_state[s].evt_type[t].avg_exec_time=time;//TODO comment this line and decomment line above 

		#if DEBUG==1
		if(lps->max_timer < lps->lp_state[s].evt_type[t].avg_exec_time)
		{
			printf("max_timer smaller than new_mean\n");
			printf("max timer=%llu,old_max=%llu,actual mean=%llu,old_mean=%llu,actual_time=%llu,s=%u,t=%u\n",
				(unsigned long long)lps->max_timer,(unsigned long long )old_max,(unsigned long long)lps->lp_state[s].evt_type[t].avg_exec_time,
				(unsigned long long)old_mean,(unsigned long long)time,s,t);
			gdb_abort;
		}
		#endif
	}
		
}

//static inline __attribute__((always_inline))
void fini_lp_stats(LP_state ** LPS, unsigned int n_prc_tot)
{
	unsigned int index;
	for (index=0; index<n_prc_tot; index++)
	{
		if (LPS[index]->lp_statistics != NULL)
		{
			unsigned int s, t;
			for (s=0; s<NUMBER_OF_LP_STATES; s++)
				for (t=0; t<NUMBER_OF_TYPES; t++)
					if (((lp_evt_stats *) LPS[index]->lp_statistics)->lp_state[s].evt_type[t].avg_exec_time != NO_TIMER)
						printf("LP-ID: %u - LP-Exe-State: %s - EVENT-Type: %u - Avg-Exe-Time: %llu\n",
							index, (s == 0) ? "Forward" : "Silent", t, (unsigned long long int) ((lp_evt_stats *) LPS[index]->lp_statistics)->lp_state[s].evt_type[t].avg_exec_time);

			free((void *) LPS[index]->lp_statistics);
			LPS[index]->lp_statistics = NULL;
		}
	}
}