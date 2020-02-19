#pragma once
#ifndef __LP_STATS_H
#define __LP_STATS_H

/*******************************
 * IN ORDER TO BE ENABLED, THE  *
 * MACRO "IPI_SUPPORT" MUST BE *
 * DEFINED AND SET TO 1.       *
 *******************************/
#if IPI_SUPPORT==1
#include <core.h>
#include <timer.h>

#define ALPHA				(0.1)
#define NUMBER_OF_TYPES		30
#define NUMBER_OF_LP_STATES	2

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

void init_lp_stats(LP_state ** LPS, unsigned int n_prc_tot);
void fini_lp_stats(LP_state ** LPS, unsigned int n_prc_tot);

static inline __attribute__((always_inline))
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

	if (lps->lp_state[s].evt_type[t].avg_exec_time == 0)
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

		lps->lp_state[s].evt_type[t].avg_exec_time = (clock_timer) ((ALPHA * ((double) time)) + ((1.0 - ALPHA) * ((double) lps->lp_state[s].evt_type[t].avg_exec_time)));

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

#endif//IPI_SUPPORT

#endif