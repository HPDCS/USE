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
			if (posix_memalign((void**)&LPS[index]->lp_statistics, 64, sizeof(lp_evt_stats)) < 0){
				printf("no memory available to allocate lp_stats\n");
				gdb_abort;
			}
		}
		memset((void *) LPS[index]->lp_statistics, 0, sizeof(lp_evt_stats));
		//static allocation end
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