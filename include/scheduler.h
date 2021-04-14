#ifndef __SCHEDULING_POLICY__
#define SCHEDULING_POLICY

#define NUMA_POLICY_GREEDY_EV 0
#define NUMA_POLICY_GREEDY_TS 1
#define NUMA_POLICY_PERIODIC  2

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

static inline void scheduler_statistics_init_skipped()						{	skipped_events=0;}
static inline void scheduler_statistics_init_current_min()					{	min_fetchable = INFTY;}
static inline void scheduler_statistics_update_current_min(simtime_t ts)	{	min_fetchable = ts;}
static inline void scheduler_statistics_update_skipped()					{	skipped_events++; }
static inline void scheduler_statistics_update_committed()					{	events_committed++; }
static inline void scheduler_init()											{	fetch_mercy_period = n_cores <<1; fetch_mercy_period_step = n_cores >> 1;}

void scheduler_update_state();


static inline bool scheduler_query(){
	#if 	NUMA_POLICY == NUMA_POLICY_GREEDY_EV
		return skipped_events > fetch_mercy_period;
	#elif 	NUMA_POLICY == NUMA_POLICY_GREEDY_TS
		return ts >= (min_fetchable + fetch_mercy_period * (bucket_width / EPB));
	#elif	NUMA_POLICY == NUMA_POLICY_PERIODIC
		return true;
	#else
		return true;
	#endif
}



#endif