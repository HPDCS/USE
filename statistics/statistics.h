#ifndef _STATISTICS_H_
#define _STATISTICS_H_


#define _clock_safe     (thread_stats[tid].clock_safe)                                  
#define avg_clock_safe  (thread_stats[tid].clock_safe/(thread_stats[tid].events_safe+1))
#define prob_safe       (thread_stats[tid].events_safe /(thread_stats[tid].events_safe+thread_stats[tid].commits_stm))
                        
#define _clock_stm      (thread_stats[tid].clock_stm)
#define avg_clock_stm   (thread_stats[tid].clock_stm /thread_stats[tid].events_stm)
//#define avg_clock_roll    (thread_stats[tid].clock_undo_event /thread_stats[tid].events_roll)
#define avg_clock_roll  (thread_stats[tid].clock_undo_event /(thread_stats[tid].events_roll+1))
#define prob_stm        (thread_stats[tid].commits_stm /(thread_stats[tid].events_safe+thread_stats[tid].commits_stm))
#define perc_util_stm   (thread_stats[tid].commit_stm/thread_stats[tid].events_stm)
#define clock_stm_util  (thread_stats[tid].clock_stm /thread_stats[tid].commit_stm)
                        
#define avg_clock_deq   (thread_stats[tid].clock_dequeue /thread_stats[tid].events_fetched)
#define avg_clock_deqlp (thread_stats[tid].clock_safety_check /thread_stats[tid].safety_check_counter)
                        
#define avg_tot_clock       (clock_timer_value(main_loop_time) / (thread_stats[tid].events_safe + thread_stats[tid].commits_stm + 1))

#define avg_time_checkpoint(stats)   (stats.clock_checkpoint / stats.counter_checkpoints)
#define avg_time_restore(stats)      (stats.clock_recovery / stats.counter_recoveries)
#define avg_time_rollback(stats)     (avg_time_checkpoint(stats) + avg_time_restore(stats))
#define avg_time_event(stats)        (stats.clock_event / stats.events_total)


#define mobile_avg(oldval, newval, samples) (oldval + (oldval + newval) / (samples))
#define percentage(part, tot) (((double)(part)) / (tot) * 100)



/* Definition of LP Statistics Post Messages */
#define STAT_EVENT                  100        /// Number of events processed
#define STAT_EVENT_REVERSE          101        /// Number of events processed in reversible mode
#define STAT_EVENT_STRAGGLER        102        /// Number of straggler events received
#define STAT_EVENT_STASH            103        /// ???
#define STAT_EVENT_SILENT           104        /// Average time to execute the silent execution phase
#define STAT_EVENT_SILENT_FOR_GVT   113        /// Average time to execute the silent execution phase
#define STAT_EVENT_COMMIT           105        /// Number of commits
#define STAT_EVENT_FETCHED          106        /// Number of call to fetched
#define STAT_EVENT_ENQUEUE          107        /// Number of events flushed
#define STAT_EVENT_UNDO             108        /// Number of events flushed
#define STAT_EVENT_ANTI             109        /// Number of canceled events already treated
#define STAT_EVENT_FETCHED_SUCC     110        /// Number of events fetched
#define STAT_EVENT_FETCHED_UNSUCC   111        /// Number of fetch failed
#define STAT_GET_NEXT_FETCH			112		   /// Number of nodes seen during fetch

#define STAT_CLOCK_EVENT            200        /// Average time to execute one event
#define STAT_CLOCK_ROLLBACK         201        /// Average time to execute one rollback
#define STAT_CLOCK_FETCH            202        /// Average time spent to fetch a new event
#define STAT_CLOCK_EVENT_REV        203        /// Average time to execute a reversible event
#define STAT_CLOCK_SILENT           204        /// Average time to execute the coasting forward operation
#define STAT_CLOCK_UNDO_EVENT       206        /// Average time to execute the reverse scrubbing operation
#define STAT_CLOCK_ENQUEUE          207        /// Average time to enqueue an event
#define STAT_CLOCK_DEQUEUE          209        /// Average time to dequeue an event
#define STAT_CLOCK_LOOP             208        /// Average time spent in the main loop
#define STAT_CLOCK_IDLE             208        /// Average time spent do nothing usefull
#define STAT_CLOCK_DEQ_LP           210        /// Average time ???
#define STAT_CLOCK_DELETE           211        /// Average time ???
#define STAT_CLOCK_PRUNE            212        /// Average time to execute the prune operation
#define STAT_CLOCK_SAFETY_CHECK     213        /// Average time to execute the safety check
#define STAT_CLOCK_BTW_EVT          214        /// Average time between two events
#define STAT_CLOCK_FETCH_SUCC       215        /// Average time spent to fetch a new event
#define STAT_CLOCK_FETCH_UNSUCC     216        /// Average time spent to fetch a new event

#define STAT_CKPT                   300        /// Number of checkpoints taken  
#define STAT_CKPT_TIME              301        /// Average time to take one checkpoint
#define STAT_CKPT_MEM               302        /// Average memory consumption used by a checkpoint
#define STAT_RECOVERY               303        /// Number of state restoration done
#define STAT_RECOVERY_TIME          304        /// Average time to restore a simulation state
#define STAT_ROLLBACK               305        /// Number of rollback operations
#define STAT_ROLLBACK_LENGTH        306        /// Number of rollback operations
#define STAT_CKPT_RECALC            307        /// Number of recalculation operations done
#define STAT_CKPT_PERIOD            308        /// Average value of the checkpoint interval
#define STAT_ONGVT                  309        /// Average value of the checkpoint interval

#define STAT_PRUNE_COUNTER          400        /// Number of pruning operations
#define STAT_SAFETY_CHECK           401        /// Number of safety check operations

#if STATISTICS_ADDED==1
//statistics without any particular macro enabled
#define STAT_EVENTS_EXEC_AND_COMMITED 900
#define STAT_CLOCK_FORWARD 901

#endif

#if POSTING==1 && REPORT==1

#define STAT_INFOS_POSTED                   700       
#define STAT_INFOS_POSTED_ATTEMPT           701
#define STAT_INFOS_POSTED_USEFUL            702
#endif

#if HANDLE_INTERRUPT==1 && REPORT==1
#define STAT_EVENT_NOT_FLUSHED              601

#define STAT_CLOCK_EXPOSITION_FORWARD       602
#define STAT_CLOCK_EXPOSITION_SILENT        603

#define STAT_EVENT_EXPOSITION_FORWARD       604
#define STAT_EVENT_EXPOSITION_SILENT        605

#define STAT_CLOCK_EXPOSITION_FORWARD_INTERRUPTED  606
#define STAT_CLOCK_EXPOSITION_SILENT_INTERRUPTED   607

#define STAT_EVENT_EXPOSITION_FORWARD_INTERRUPTED  608
#define STAT_EVENT_EXPOSITION_SILENT_INTERRUPTED   609
#endif

#if HANDLE_INTERRUPT_WITH_CHECK==1
#define STAT_SYNC_CHECK_SILENT            	800
#define STAT_SYNC_CHECK_FORWARD           	801
#define STAT_SYNC_CHECK_USEFUL              802
#endif

#if IPI_SUPPORT && REPORT==1
#define STAT_IPI_SENDED             500
#define STAT_IPI_RECEIVED           501 
#define STAT_IPI_SYSCALL_TIME       502
#define STAT_IPI_TRAMPOLINE_RECEIVED 503 //this statistic is not used explicitly, it is used in trampoline.S
#endif

typedef double stat64_t;

struct stats_t {
    stat64_t events_total;
    stat64_t events_straggler;
    stat64_t events_committed;
    stat64_t events_reverse;
    stat64_t events_stash;
    stat64_t events_fetched;		//number of fethc call
    stat64_t events_fetched_succ;	//number of events fetched
    stat64_t events_fetched_unsucc;	//number of fetch failed
    stat64_t events_enqueued;
    stat64_t events_silent;
    stat64_t events_silent_for_gvt;
    stat64_t events_undo;
    stat64_t events_anti;
    stat64_t events_get_next_fetch;

    stat64_t counter_rollbacks;
    stat64_t counter_rollbacks_length;
    stat64_t counter_recoveries;
    stat64_t counter_checkpoints;
    stat64_t counter_prune;
    stat64_t counter_safety_check;
    stat64_t counter_checkpoint_recalc;
    stat64_t counter_ongvt;
    
    stat64_t clock_event;
    stat64_t clock_fetch;
    stat64_t clock_fetch_succ;//PADS2018
    stat64_t clock_fetch_unsucc;//PADS2018
    stat64_t clock_stm_wait;
    stat64_t clock_undo_event;
    stat64_t clock_enqueue;
    stat64_t clock_dequeue;
    stat64_t clock_deq_lp;
    stat64_t clock_loop;
    stat64_t clock_delete;
    stat64_t clock_prune;
    stat64_t clock_safety_check;
    stat64_t clock_silent;	//PADS2018
    stat64_t clock_safe;	//PADS2018
    stat64_t clock_recovery;
    stat64_t clock_checkpoint;
    stat64_t clock_rollback;

    stat64_t mem_checkpoint;
    stat64_t checkpoint_period;

    stat64_t total_frames;
    
    
    stat64_t clock_loop_tot;
    stat64_t clock_event_tot;
    stat64_t clock_safe_tot;
    stat64_t clock_frame_tot;

    #if STATISTICS_ADDED==1
    //new statistics added
    stat64_t clock_forward_exec_lp;//per LP
    stat64_t clock_forward_exec_per_event;//per event
    stat64_t clock_forward_exec_tot;

    stat64_t events_exec_and_committed_lp;//per LP
    stat64_t events_exec_and_committed_tot;
    #endif

    #if POSTING==1 && REPORT==1
    
    stat64_t infos_posted_tid;//per thread num info posted by thread
    stat64_t infos_posted_attempt_tid;//per thread num tries to post info by thread
    stat64_t infos_posted_useful_tid;//per thread num info useful for lp
    
    stat64_t infos_posted_tot;
    stat64_t infos_posted_attempt_tot;
    stat64_t infos_posted_useful_tot;
    
    #endif
    #if HANDLE_INTERRUPT==1 && REPORT==1
    stat64_t event_not_flushed_lp;//per lp, event that lp father doesn't flush
    stat64_t event_not_flushed_tot;

    stat64_t event_exposition_forward_lp;//per LP
    stat64_t event_exposition_silent_lp;//per LP

    stat64_t event_exposition_forward_tot;
    stat64_t event_exposition_silent_tot;

    stat64_t clock_exposition_forward_per_event;//per event
    stat64_t clock_exposition_silent_per_event;//per event

    stat64_t clock_exposition_forward_tot;
    stat64_t clock_exposition_silent_tot;

    //statistics related to interruptions
    stat64_t event_exposition_forward_interrupted_lp;//per LP
    stat64_t event_exposition_silent_interrupted_lp;//per LP

    stat64_t event_exposition_forward_interrupted_tot;
    stat64_t event_exposition_silent_interrupted_tot;

    stat64_t clock_exposition_forward_interrupted_per_event;//per event
    stat64_t clock_exposition_silent_interrupted_per_event;//per event

    stat64_t clock_exposition_forward_interrupted_tot;
    stat64_t clock_exposition_silent_interrupted_tot;
    #endif

    #if HANDLE_INTERRUPT_WITH_CHECK==1
    stat64_t sync_check_silent_lp;//per lp, num sync_check in past maded by lp
    stat64_t sync_check_forward_lp;//per lp, num sync_check in future maded by lp

    stat64_t sync_check_silent_tot;
    stat64_t sync_check_forward_tot;

    stat64_t sync_check_useful_lp;//per lp num sync_check useful maded by lp
    stat64_t sync_check_useful_tot;
    #endif

    #if IPI_SUPPORT==1 && REPORT==1
    stat64_t ipi_sended_tid;//per thread, num of ipis sended by target thread
    stat64_t ipi_trampoline_received_tid;//per thread, num of ipis handled in trampoline
    stat64_t ipi_received_tid;//per thread, num of ipis received by target thread

    //calculated in gather statistics
    stat64_t ipi_sended_tot;
    stat64_t ipi_trampoline_received_tot;
    stat64_t ipi_received_tot;

    stat64_t clock_exec_ipi_syscall_tid; //per thread
    stat64_t clock_exec_ipi_syscall_tot;

    #endif
    
} __attribute__((aligned (64)));



/*struct stats_lp_t {
    stat64_t events_total;
    stat64_t events_straggler;
    stat64_t events_committed;
    stat64_t events_reverse;
    stat64_t events_stash;
    stat64_t events_safe_silent;//PADS2018
    stat64_t events_fetched;
    stat64_t events_flushed;
    stat64_t events_reprocessed;

    stat64_t counter_rollbacks;
    stat64_t counter_recoveries;
    stat64_t counter_checkpoints;
    stat64_t counter_prune;
    stat64_t counter_safety_check;
    stat64_t counter_checkpoint_recalc;
    
    stat64_t clock_event;
    stat64_t clock_fetch;
    stat64_t clock_stm_wait;
    stat64_t clock_undo_event;
    stat64_t clock_enqueue;
    stat64_t clock_dequeue;
    stat64_t clock_deq_lp;
    stat64_t clock_loop;
    stat64_t clock_delete;
    stat64_t clock_prune;
    stat64_t clock_safety_check;
    stat64_t clock_safe_silent;//PADS2018
    stat64_t clock_recovery;
    stat64_t clock_checkpoint;
    stat64_t clock_rollback;

    stat64_t mem_checkpoint;

    unsigned int checkpoint_period;
} __attribute__((aligned (64)));*/

extern struct stats_t *thread_stats;
extern struct stats_t *lp_stats;
extern struct stats_t *system_stats;

enum stat_levels {STATS_GLOBAL, STATS_PERF, STATS_LP, STATS_ALL};


void statistics_init();

void statistics_fini();

void print_statistics();

//void statistics_post_data(int lid, int type, stat64_t value);

void statistics_post_lp_data(unsigned int lid, int type, stat64_t value);

void statistics_post_th_data(unsigned int lid, int type, stat64_t value);

#endif // _STATISTICS_H_
