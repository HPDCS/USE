#pragma once
#ifndef __IPI_DECISION_MODEL_STATS_H
#define __IPI_DECISION_MODEL_STATS_H

/*******************************
 * IN ORDER TO BE ENABLED, THE  *
 * MACROS "IPI_DECISION_MODEL" MUST BE *
 * DEFINED AND SET TO 1.       *
 *******************************/
#if IPI_DECISION_MODEL==1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <timer.h>
#include <core.h>
#include <simtypes.h>
#include <hpdcs_utils.h>
#include <handle_interrupt.h>

#define ALPHA				(0.5)
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

void init_ipi_decision_model_stats();
void fini_ipi_decision_model_stats();

void print_ipi_decision_model_stats();

static inline __attribute__((always_inline))
clock_timer get_actual_mean(unsigned int lp_idx,unsigned int s, unsigned int t){
	s = (s != LP_STATE_READY) ? 1 : 0;
	lp_evt_stats *lps=LPS[lp_idx]->ipi_statistics;
	return lps->lp_state[s].evt_type[t].avg_exec_time;
}

static inline __attribute__((always_inline))
void store_ipi_decision_model_stats(unsigned int lp_idx, unsigned int s, unsigned int t, clock_timer time)
{
	lp_evt_stats *lps=LPS[lp_idx]->ipi_statistics;
	#if DEBUG==1
	if( (s!=LP_STATE_READY) && (s!=LP_STATE_SILENT_EXEC) ){
		printf("invalid LP_mode_state\n");
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

#endif//IPI_DECISION_MODEL==1

#endif