#pragma once
#ifndef __LP_STATS_H
#define __LP_STATS_H

/*******************************
 * IN ORDER TO BE ENABLED THE  *
 * MACRO "IPI_SUPPORT" MUST BE *
 * DEFINED AND SET TO 1.       *
 *******************************/


#if HANDLE_INTERRUPT==1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <core.h>
#include <simtypes.h>

#ifndef RDTSC
#define RDTSC() ({ \
	unsigned int cycles_low; \
	unsigned int cycles_high; \
	asm volatile ( \
		"RDTSC\n\t" \
		"mov %%edx, %0\n\t" \
		"mov %%eax, %1\n\t" \
		: \
		"=r" (cycles_high), "=r" (cycles_low) \
		: \
		: \
		"%rax", "%rdx" \
	); \
	(((unsigned long long int) cycles_high << 32) | cycles_low); \
})
#endif
#endif

# if IPI_SUPPORT==1

#define ALPHA				0.3
#define NUMBER_OF_TYPES		30
#define NUMBER_OF_STATES	2

struct _lp_state_type_stats {
	double avg_exec_time;
};

struct _lp_state_stats {
	struct _lp_state_type_stats evt_type[NUMBER_OF_TYPES];
};

typedef struct _lp_stats {
	struct _lp_state_stats lp_state[NUMBER_OF_STATES];
} lp_evt_stats;

static inline __attribute__((always_inline))
void init_lp_stats(LP_state ** LPS, unsigned int n_prc_tot)
{
	unsigned int index;
	if (LPS == NULL)
		return;
	for (index=0; index<n_prc_tot; index++)
	{
		if (LPS[index] == NULL)
			continue;
		if (LPS[index]->lp_statistics == NULL)
			if ((LPS[index]->lp_statistics = malloc(sizeof(lp_evt_stats))) == NULL)
				continue;
		memset((void *) LPS[index]->lp_statistics, 0, sizeof(lp_evt_stats));
	}
}

static inline __attribute__((always_inline))
void store_lp_stats(lp_evt_stats *lps, unsigned int s, unsigned int t, unsigned long long time)
{
	s = (s == LP_STATE_SILENT_EXEC) ? 1 : 0;
	lps->lp_state[s].evt_type[t].avg_exec_time += ALPHA * (((double) time) - lps->lp_state[s].evt_type[t].avg_exec_time);
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
			for (s=0; s<NUMBER_OF_STATES; s++)
				for (t=0; t<NUMBER_OF_TYPES; t++)
					if (((lp_evt_stats *) LPS[index]->lp_statistics)->lp_state[s].evt_type[t].avg_exec_time != 0.0)
						printf("LP-ID: %u - LP-Exe-State: %s - EVENT-Type: %u - Avg-Exe-Time: %llu\n",
							index, (s == 0) ? "Forward" : "Silent", t, (unsigned long long int) ((lp_evt_stats *) LPS[index]->lp_statistics)->lp_state[s].evt_type[t].avg_exec_time);

			free((void *) LPS[index]->lp_statistics);
			LPS[index]->lp_statistics = NULL;
		}
	}
}

# endif
#endif