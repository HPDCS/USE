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
void store_lp_stats(unsigned int lp_idx, unsigned int s, unsigned int t, clock_timer time);
void fini_lp_stats(LP_state ** LPS, unsigned int n_prc_tot);

#endif//IPI_SUPPORT

#endif