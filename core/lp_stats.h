#pragma once
#ifndef __LP_STATS_H
#define __LP_STATS_H

/*******************************
 * IN ORDER TO BE ENABLED THE  *
 * MACRO "IPI_SUPPORT" MUST BE *
 * DEFINED AND SET TO 1.       *
 *******************************/
#if IPI_SUPPORT==1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <core.h>
#include <simtypes.h>
#include <timer.h>

#define ALPHA				0.1
#define NUMBER_OF_TYPES		30
#define NUMBER_OF_LP_STATES	2


#define NO_TIMER 0ULL

struct _lp_state_type_stats {
	clock_timer avg_exec_time;
};

struct _lp_state_stats {
	struct _lp_state_type_stats evt_type[NUMBER_OF_TYPES];
};

typedef struct _lp_stats {
	struct _lp_state_stats lp_state[NUMBER_OF_LP_STATES];
	#if DEBUG==1
	clock_timer max_timer;
	#endif
} lp_evt_stats;

/*struct _lp_state_type_stats {
	clock_timer avg_exec_time;
};

struct _lp_state_stats {
	struct _lp_state_type_stats *evt_type;//[NUMBER_OF_TYPES]
};

typedef struct _lp_stats {
	struct _lp_state_stats *lp_state;//[NUMBER_OF_LP_STATES]
	#if DEBUG==1
	clock_timer max_timer;
	#endif
} lp_evt_stats;*/

static inline __attribute__((always_inline))
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

		//dynamic allocation start,decoment struct definition
		/*LPS[index]->lp_statistics = malloc(sizeof(lp_evt_stats));
		if(LPS[index]->lp_statistics==NULL){
			printf("no memory available to allocate lp_stats\n");
			gdb_abort;
		}

		lp_evt_stats *lp_stats=LPS[index]->lp_statistics;

		lp_stats->lp_state= malloc(sizeof(struct _lp_state_stats)*NUMBER_OF_LP_STATES);
		if(lp_stats->lp_state==NULL){
			printf("no memory available to allocate lp_stats\n");
			gdb_abort;
		}
			
		for(int i=0;i<NUMBER_OF_LP_STATES;i++){
			lp_stats->lp_state[i].evt_type=malloc(sizeof(struct _lp_state_type_stats)*NUMBER_OF_TYPES);
			if(lp_stats->lp_state[i].evt_type==NULL){
				printf("no memory available to allocate lp_stats\n");
				gdb_abort;
			}
			memset(lp_stats->lp_state[i].evt_type,NO_TIMER,sizeof(struct _lp_state_type_stats));
		}*/
		//dynamic allocation end



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

static inline __attribute__((always_inline))
void store_lp_stats(lp_evt_stats *lps, unsigned int s, unsigned int t, clock_timer time)
{
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
			printf("max timer=%llu,old_max=%llu,old_mean=%llu,actual_time=%llu\n",
				(unsigned long long)lps->max_timer,(unsigned long long )old_max,
				(unsigned long long)old_mean,(unsigned long long)time);
			gdb_abort;
		}
		#endif

		lps->lp_state[s].evt_type[t].avg_exec_time = time; // (clock_timer) ((ALPHA * ((double) time)) + ((1.0 - ALPHA) * ((double) lps->lp_state[s].evt_type[t].avg_exec_time)));
		
		#if DEBUG==1
		if(lps->max_timer < lps->lp_state[s].evt_type[t].avg_exec_time)
		{
			printf("max_timer smaller than new_mean\n");
			printf("max timer=%llu,old_max=%llu,actual mean=%llu,old_mean=%llu,actual_time=%llu\n",
				(unsigned long long)lps->max_timer,(unsigned long long )old_max,(unsigned long long)lps->lp_state[s].evt_type[t].avg_exec_time,
				(unsigned long long)old_mean,(unsigned long long)time);
			gdb_abort;
		}
		#endif
	}
		
}

static inline __attribute__((always_inline))
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

#endif//IPI_SUPPORT

#endif