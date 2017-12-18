#ifndef _STATISTICS_H_
#define _STATISTICS_H_


#define _clock_safe		(thread_stats[tid].clock_safe)									
#define avg_clock_safe	(thread_stats[tid].clock_safe/(thread_stats[tid].events_safe+1))
#define prob_safe		(thread_stats[tid].events_safe /(thread_stats[tid].events_safe+thread_stats[tid].commits_stm))
                        
#define _clock_stm		(thread_stats[tid].clock_stm)
#define avg_clock_stm	(thread_stats[tid].clock_stm /thread_stats[tid].events_stm)
//#define avg_clock_roll	(thread_stats[tid].clock_undo_event /thread_stats[tid].events_roll)
#define avg_clock_roll	(thread_stats[tid].clock_undo_event /(thread_stats[tid].events_roll+1))
#define prob_stm		(thread_stats[tid].commits_stm /(thread_stats[tid].events_safe+thread_stats[tid].commits_stm))
#define perc_util_stm	(thread_stats[tid].commit_stm/thread_stats[tid].events_stm)
#define clock_stm_util	(thread_stats[tid].clock_stm /thread_stats[tid].commit_stm)
                        
#define avg_clock_deq	(thread_stats[tid].clock_dequeue /thread_stats[tid].events_fetched)
#define avg_clock_deqlp	(thread_stats[tid].clock_safety_check /thread_stats[tid].safety_check_counter)
                        
#define avg_tot_clock		(clock_timer_value(main_loop_time) / (thread_stats[tid].events_safe + thread_stats[tid].commits_stm + 1))

#define EVENTS_SAFE 0
#define EVENTS_HTM 1 //DEL
#define EVENTS_STM 2
#define EVENTS_ROLL 30
#define EVENTS_STASH 34

#define COMMITS_HTM 3 //DEL
#define COMMITS_STM 4

#define CLOCK_SAFE 5
#define CLOCK_HTM 6 //DEL
#define CLOCK_STM 7

#define CLOCK_HTM_THROTTLE 8 //DEL
#define CLOCK_STM_WAIT 9
#define CLOCK_UNDO_EVENT 10
#define CLOCK_ENQUEUE 22
#define CLOCK_LOOP 24
#define CLOCK_DEQUEUE 23
#define CLOCK_DEQ_LP 31
#define CLOCK_DELETE 32
#define CLOCK_PRUNE 35
#define CLOCK_SAFETY_CHECK 37

#define ABORT_UNSAFE 11		//DEL
#define ABORT_REVERSE 12    //DEL
#define ABORT_CONFLICT 13   //DEL
#define ABORT_CACHEFULL 14  //DEL
#define ABORT_DEBUG 15      //DEL
#define ABORT_NESTED 16     //DEL
#define ABORT_GENERIC 17    //DEL
#define ABORT_RETRY 18      //DEL
#define ABORT_TOTAL 19      //DEL

#define EVENTS_FETCHED 20
#define EVENTS_FLUSHED 33
#define PRUNE_COUNTER 36
#define SAFETY_CHECK 38
#define T_BTW_EVT 21



/* Definition of LP Statistics Post Messages */
#define STAT_ANTIMESSAGE    41
#define STAT_EVENT          42
#define STAT_COMMITTED      43
#define STAT_ROLLBACK       44
#define STAT_CKPT           45
#define STAT_CKPT_TIME      46
#define STAT_CKPT_MEM       47
#define STAT_RECOVERY       48
#define STAT_RECOVERY_TIME  49
#define STAT_EVENT_TIME     50
#define STAT_IDLE_CYCLES    51
#define STAT_SILENT         52


struct stats_t {
    unsigned int events_total;
    unsigned int events_safe;
    unsigned int events_htm;//
    unsigned int events_stm;
    unsigned int events_roll;
    unsigned int events_stash;
    
    unsigned int commits_htm;//
    unsigned int commits_stm;

    unsigned long long clock_safe;
    unsigned long long clock_htm;//
    unsigned long long clock_stm;
    unsigned long long clock_htm_throttle;//
    unsigned long long clock_stm_wait;
    unsigned long long clock_undo_event;
    unsigned long long clock_enqueue;
    unsigned long long clock_dequeue;
    unsigned long long clock_deq_lp;
    unsigned long long clock_loop;
    unsigned long long clock_delete;
    unsigned long long clock_prune;
    unsigned long long clock_safety_check;

    unsigned int abort_total;//
    unsigned int abort_unsafe;//
    unsigned int abort_reverse;//
    unsigned int abort_conflict;//
    unsigned int abort_cachefull;//
    unsigned int abort_debug;//
    unsigned int abort_nested;//
    unsigned int abort_generic;//
    unsigned int abort_retry;//
    
	unsigned long long events_fetched;
	unsigned long long events_flushed;
	unsigned long long prune_counter;
	unsigned long long safety_check_counter;
} __attribute__((aligned (64)));



struct stats_lp_t {
    unsigned int events_total;
    unsigned int events_safe;
    unsigned int events_htm;//
    unsigned int events_stm;
    unsigned int events_roll;
    unsigned int events_stash;
    
    unsigned int commits_htm;//
    unsigned int commits_stm;

    unsigned long long clock_safe;
    unsigned long long clock_htm;//
    unsigned long long clock_stm;
    unsigned long long clock_htm_throttle;//
    unsigned long long clock_stm_wait;
    unsigned long long clock_undo_event;
    unsigned long long clock_enqueue;
    unsigned long long clock_dequeue;
    unsigned long long clock_deq_lp;
    unsigned long long clock_loop;
    unsigned long long clock_delete;
    unsigned long long clock_prune;
    unsigned long long clock_safety_check;

    unsigned int abort_total;//
    unsigned int abort_unsafe;//
    unsigned int abort_reverse;//
    unsigned int abort_conflict;//
    unsigned int abort_cachefull;//
    unsigned int abort_debug;//
    unsigned int abort_nested;//
    unsigned int abort_generic;//
    unsigned int abort_retry;//
    
    unsigned long long events_fetched;
    unsigned long long events_flushed;
    unsigned long long prune_counter;
    unsigned long long safety_check_counter;
} __attribute__((aligned (64)));


extern struct stats_t *thread_stats;

enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};


void statistics_init();

void statistics_fini();

void print_statistics();

void statistics_post_data(int lid, int type, double value);

void statistics_post_lp_data(int lid, int type, double value);

#endif // _STATISTICS_H_
