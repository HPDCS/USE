#if IPI_DECISION_MODEL==1
#include <ipi_decision_model_stats.h>

void init_ipi_decision_model_stats()
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
		if (LPS[index]->ipi_statistics == NULL){
			if (posix_memalign((void**)&LPS[index]->ipi_statistics, 64, sizeof(lp_evt_stats)) < 0){
				printf("no memory available to allocate lp_stats\n");
				gdb_abort;
			}
		}
		memset((void *) LPS[index]->ipi_statistics, 0, sizeof(lp_evt_stats));
	}
}

#if PRINT_IPI_DECISION_MODEL_STATS==1
void print_ipi_decision_model_stats()
{
	unsigned int index;
	for (index=0; index<n_prc_tot; index++)
	{
		if (LPS[index]->ipi_statistics != NULL)
		{
			unsigned int s, t;
			for (s=0; s<NUMBER_OF_LP_STATES; s++){
				for (t=0; t<NUMBER_OF_TYPES; t++){
					if (((lp_evt_stats *) LPS[index]->ipi_statistics)->lp_state[s].evt_type[t].avg_exec_time != 0)
						printf("LP-ID: %u - LP-Exe-State: %s - EVENT-Type: %u - Avg-Exe-Time: %llu\n",
						index, (s == 0) ? "Forward" : "Silent", t, (unsigned long long int) ((lp_evt_stats *) LPS[index]->ipi_statistics)->lp_state[s].evt_type[t].avg_exec_time);
				}
			}	
			#if DEBUG==1
			printf("LP-ID: %u max_timer=%llu\n",index,(unsigned long long)((lp_evt_stats*)LPS[index]->ipi_statistics)->max_timer);
			#endif
		}
	}
}
#endif

void fini_ipi_decision_model_stats()
{
	unsigned int index;
	for (index=0; index<n_prc_tot; index++)
	{
		free((void *) LPS[index]->ipi_statistics);
		LPS[index]->ipi_statistics = NULL;
	}
}

#endif