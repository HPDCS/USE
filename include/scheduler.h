#ifndef __SCHEDULING_POLICY__
#define SCHEDULING_POLICY


#include <timer.h>

#define NUMA_POLICY_GREEDY_EV 0
#define NUMA_POLICY_GREEDY_TS 1
#define NUMA_POLICY_PERIODIC  2

/*
#if DISTRIBUTED_FETCH == 1
	#ifndef NUMA_POLICY
		#define NUMA_POLICY NUMA_POLICY_GREEDY_EV
	#endif
#endif 
*/
extern __thread unsigned int fetch_mercy_period;
extern __thread int fetch_mercy_period_step; //se negativo, indica che lo sto riducendo

extern __thread unsigned long long curr_clock;
extern __thread unsigned long long prev_clock;

extern __thread double curr_throughput;
extern __thread double prev_throughput;

extern __thread unsigned int prev_committed;
extern __thread unsigned int curr_committed;
extern __thread unsigned int events_committed;

extern __thread unsigned int mercy_period_recalc;
extern __thread unsigned int skipped_events;
extern __thread simtime_t min_fetchable;




extern __thread unsigned long long avg_clock_remote_execution; //eliminabile
extern __thread unsigned long long avg_clock_local_execution; //eliminabile
extern __thread unsigned long long tot_clock_local_execution;
extern __thread unsigned int num_local_execution;
extern __thread unsigned long long tot_clock_remote_execution;
extern __thread unsigned int num_remote_execution;
extern __thread unsigned long long tot_clock_remote_fetch;
extern __thread unsigned int num_remote_fetch;
extern __thread unsigned long long avg_clock_remote_fetch;
extern __thread unsigned long long tot_clock_local_fetch;
extern __thread unsigned int num_local_fetch;
extern __thread unsigned long long avg_clock_local_fetch;
extern __thread bool partitioned_LP;
extern __thread unsigned int decision_period;
extern __thread unsigned int decision_period_lenght;



static inline void scheduler_statistics_init_skipped()						{	skipped_events=0;}
static inline void scheduler_statistics_init_current_min()					{	min_fetchable = INFTY;}
static inline void scheduler_statistics_update_current_min(simtime_t ts)	{	min_fetchable = ts;}
static inline void scheduler_statistics_update_skipped()					{	skipped_events++; }
static inline void scheduler_statistics_update_committed()					{	events_committed++; }
static inline void scheduler_init()											{	fetch_mercy_period = n_cores <<1; fetch_mercy_period_step = n_cores >> 1;}


void scheduler_statistics_update_execution_times(clock_timer event_processing_timer);
void scheduler_statistics_update_fetch_times(clock_timer fetch_timer);		

void scheduler_update_state();


static inline bool scheduler_query(){
	#if 	NUMA_POLICY == NUMA_POLICY_GREEDY_EV
		return skipped_events > fetch_mercy_period;
	#elif 	NUMA_POLICY == NUMA_POLICY_GREEDY_TS
		return ts >= (min_fetchable + fetch_mercy_period * (bucket_width / EPB));
	#elif	NUMA_POLICY == NUMA_POLICY_PERIODIC
		return !partitioned_LP;
	#else
		return true;
	#endif
}



#endif