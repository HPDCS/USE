#include <stdio.h>

#include <statistics.h>
#include <core.h>
#include <hpdcs_utils.h>
#include <simtypes.h>


//Variabili da tunare durante l'esecuzione per throttling e threshold
extern double delta_count;
extern unsigned int reverse_execution_threshold;

struct stats_t *thread_stats;
struct stats_t *lp_stats;
struct stats_t *system_stats;

void statistics_init() {
	if(posix_memalign((void**)&thread_stats, 64, n_cores * sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n");
	}
	if(thread_stats == NULL) {
		printf("Unable to allocate thread statistics vector\n");
		abort();
	}

	if(posix_memalign((void**)&lp_stats, 64, n_prc_tot * sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n");
	}
	if(lp_stats == NULL) {
		printf("Unable to allocate LP statistics vector\n");
		abort();
	}

	if(posix_memalign((void**)&system_stats, 64, sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n");
	}
	if(lp_stats == NULL) {
		printf("Unable to allocate LP statistics vector\n");
		abort();
	}

	memset(thread_stats, 0, n_cores * sizeof(struct stats_t));
	memset(lp_stats, 0, n_prc_tot * sizeof(struct stats_t));
	memset(system_stats, 0, sizeof(struct stats_t));
}

void statistics_fini() {
	free(thread_stats);
	free(lp_stats);
	free(system_stats);
}

static void statistics_post_data(struct stats_t *stats, int idx, int type, stat64_t value) {

	switch(type) {
		// ========= EVENTS STATS ========= //
		case STAT_EVENT:
			stats[idx].events_total += value;
			break;

		case STAT_EVENT_COMMIT:
			stats[idx].events_committed += value;
			break;
			
		case STAT_EVENT_STRAGGLER:
			stats[idx].events_straggler += value;
			break;

		case STAT_EVENT_STASH:
			stats[idx].events_stash += value;
			break;

		case STAT_EVENT_SILENT_FOR_GVT:
			stats[idx].events_silent_for_gvt += value;
			break;


		case STAT_EVENT_SILENT:
			stats[idx].events_silent += value;
			break;

		case STAT_EVENT_UNDO:
			stats[idx].events_undo += value;
			break;

		case STAT_EVENT_FETCHED:
			stats[idx].events_fetched += value;
			break;

		case STAT_EVENT_ENQUEUE:
			stats[idx].events_enqueued += value;
			break;

		case STAT_EVENT_ANTI:
			stats[idx].events_anti += value;
			break;

		case STAT_EVENT_FETCHED_SUCC:
			stats[idx].events_fetched_succ += value;
			break;

		case STAT_EVENT_FETCHED_UNSUCC:
			stats[idx].events_fetched_unsucc += value;
			break;
			
		case STAT_GET_NEXT_FETCH:
			stats[idx].events_get_next_fetch += value;
			break;


		// ========= COUNTER STATS ========= //
		case STAT_PRUNE_COUNTER:
			stats[idx].counter_prune++;
			break;

		case STAT_SAFETY_CHECK:
			stats[idx].counter_safety_check++;
			break;

		case STAT_ROLLBACK:
			stats[idx].counter_rollbacks += value;
			break;

		case STAT_ROLLBACK_LENGTH:
			stats[idx].counter_rollbacks_length += value;
			break;

		case STAT_ONGVT:
			stats[idx].counter_ongvt += value;
			break;

		case STAT_CKPT:
			stats[idx].counter_checkpoints += value;
			break;

		case STAT_CKPT_MEM:
			stats[idx].mem_checkpoint += value;
			//stats[idx].mem_checkpoint /= stats[idx].counter_checkpoints;
			break;

		case STAT_RECOVERY:
			stats[idx].counter_recoveries += value;
			break;

		case STAT_CKPT_RECALC:
			stats[idx].counter_checkpoint_recalc += value;
			break;

		case STAT_CKPT_PERIOD:
			stats[idx].checkpoint_period += (value - stats[idx].checkpoint_period) / stats[idx].counter_checkpoint_recalc;
			//stats[idx].checkpoint_period /= stats[idx].counter_checkpoint_recalc;
			break;


		// ========= TIME STATS ========= //
		case STAT_CLOCK_EVENT:
			stats[idx].clock_event += value;
			//stats[idx].clock_event /= stats[idx].events_total;
			break;

		case STAT_CLOCK_DEQUEUE:
			stats[idx].clock_dequeue += value;
			break;

		//case STAT_CLOCK_FETCH:
		//	stats[idx].clock_fetch += value;
		//	break;

		case STAT_CLOCK_DEQ_LP:
			stats[idx].clock_deq_lp += value;
			break;

		case STAT_CLOCK_ENQUEUE:
			stats[idx].clock_enqueue += value;
			break;

		case STAT_CLOCK_DELETE:
			stats[idx].clock_delete += value;
			break;

		case STAT_CLOCK_LOOP:
			stats[idx].clock_loop += value;
			break;

		case STAT_CLOCK_UNDO_EVENT:
			stats[idx].clock_undo_event += value;
			break;

		case STAT_CLOCK_PRUNE:
			stats[idx].clock_prune += value;
			break;

		case STAT_CLOCK_SAFETY_CHECK:
			stats[idx].clock_safety_check += value;
			break;
		
		case STAT_CLOCK_SILENT:
			stats[idx].clock_silent += value;
			break;

		case STAT_CLOCK_ROLLBACK:
			stats[idx].clock_rollback += value;
			break;

		case STAT_CLOCK_BTW_EVT:
//			tot_time_between_events += value;
//			t_btw_evts = tot_time_between_events/events_fetched;
			break;

		case STAT_CKPT_TIME:
			stats[idx].clock_checkpoint += value;
			//stats[idx].clock_checkpoint /= stats[idx].counter_checkpoints;
			break;

		case STAT_RECOVERY_TIME:
			stats[idx].clock_recovery += value;
			//stats[idx].clock_recovery /= stats[idx].counter_recoveries;
			break;

		case STAT_CLOCK_FETCH_SUCC:
			stats[idx].clock_fetch_succ += value;
			break;

		case STAT_CLOCK_FETCH_UNSUCC:
			stats[idx].clock_fetch_unsucc += value;
			break;

		#if IPI_SUPPORT==1 && REPORT==1

		case STAT_IPI_SENDED:
			stats[idx].ipi_sended += value;
			break;
		case STAT_IPI_RECEIVED:
			stats[idx].ipi_received += value;
			break;

		#endif

		#if POSTING==1 && REPORT==1

		case STAT_EVENT_NOT_FLUSHED:
			stats[idx].event_not_flushed += value;
			break;
		case STAT_INFOS_POSTED:
			stats[idx].infos_posted += value;
			break;
		case STAT_INFOS_POSTED_ATTEMPT:
			stats[idx].infos_posted_attempt += value;
			break;
		case STAT_INFOS_POSTED_USEFUL:
			stats[idx].infos_posted_useful += value;
			break;
		case STAT_SYNC_CHECK_SILENT:
			stats[idx].sync_check_silent += value;
			break;
		case STAT_SYNC_CHECK_FORWARD:
			stats[idx].sync_check_forward += value;
			break;

		#endif
		
		#if POSTING==1 || IPI_SUPPORT==1 && REPORT==1
		case STAT_SYNC_CHECK_USEFUL:
			stats[idx].sync_check_useful += value;
			break;
		case STAT_EVENT_FORWARD_INTERRUPTED:
			stats[idx].event_forward_interrupted += value;
			break;
		case STAT_EVENT_SILENT_INTERRUPTED:
			stats[idx].event_silent_interrupted += value;
			break;
		case STAT_CLOCK_EXEC_EVT_INTER_FORWARD_EXEC:
			stats[idx].clock_exec_evt_inter_forward_exec += value;
			break;
		case STAT_CLOCK_EXEC_EVT_INTER_SILENT_EXEC:
			stats[idx].clock_exec_evt_inter_silent_exec += value;
			break;
		case STAT_IPI_SYSCALL_TIME:
			stats[idx].clock_exec_ipi_syscall += value;
			break;
		#endif

		case STAT_EVENTS_EXEC_AND_COMMITED:
			stats[idx].events_exec_and_committed += value;//per LP
			break;
		case STAT_CLOCK_FORWARD:
			stats[idx].clock_forward_exec += value;
			break;
		default:
			printf("Unrecognized stat type (%d)\n", type);
	}
}

inline void statistics_post_lp_data(unsigned int lp_idx, int type, stat64_t value) {
	assert(lp_idx < n_prc_tot);
	statistics_post_data(lp_stats, lp_idx, type, value);
}

inline void statistics_post_th_data(unsigned int tid, int type, stat64_t value) {
	assert(tid < n_cores);
	statistics_post_data(thread_stats, tid, type, value);
}


void gather_statistics() {
	unsigned int i;

	// Aggregate per thread
	for(i = 0; i < n_cores; i++) {
		system_stats->events_committed     += thread_stats[i].events_committed;
		
		system_stats->counter_prune        += thread_stats[i].counter_prune;
		//system_stats->counter_ongvt        += thread_stats[i].counter_ongvt;

		//system_stats->clock_enqueue        += thread_stats[i].clock_enqueue;
		//system_stats->clock_dequeue        += thread_stats[i].clock_dequeue;
		//system_stats->clock_deq_lp         += thread_stats[i].clock_deq_lp;
		//system_stats->clock_loop           += thread_stats[i].clock_loop;
		system_stats->clock_prune          += thread_stats[i].clock_prune;
		system_stats->clock_loop           += thread_stats[i].clock_loop;
		
		system_stats->events_fetched       += thread_stats[i].events_fetched;
		system_stats->events_fetched_unsucc+= thread_stats[i].events_fetched_unsucc;
		system_stats->clock_fetch_unsucc   += thread_stats[i].clock_fetch_unsucc;
		system_stats->events_get_next_fetch+= thread_stats[i].events_get_next_fetch;
		
		#if POSTING==1 && REPORT==1
		system_stats->infos_posted += 		thread_stats[i].infos_posted;//per thread num info posted by thread
		system_stats->infos_posted_attempt += 		thread_stats[i].infos_posted_attempt;//per thread num info posted by thread
		#endif
		#if IPI_SUPPORT && REPORT==1
		system_stats->ipi_sended += thread_stats[i].ipi_sended;
		system_stats->ipi_trampoline_received += thread_stats[i].ipi_trampoline_received;
		system_stats->ipi_received += thread_stats[i].ipi_received;
		system_stats->clock_exec_ipi_syscall_tot += thread_stats[i].clock_exec_ipi_syscall;
		#endif
		
	}

	system_stats->clock_prune /= system_stats->counter_prune;
	system_stats->clock_loop_tot = system_stats->clock_loop;
	system_stats->clock_loop /= n_cores;
	
	#if IPI_SUPPORT==1 && REPORT==1
	system_stats->ipi_sended_tot = system_stats->ipi_sended;
	system_stats->ipi_trampoline_received_tot = system_stats->ipi_trampoline_received;
	system_stats->ipi_received_tot = system_stats->ipi_received;

	system_stats->ipi_sended /= n_cores;
	system_stats->ipi_trampoline_received /= n_cores;
	system_stats->ipi_received /= n_cores;

	#endif

	#if POSTING==1 && REPORT==1
	system_stats->infos_posted_tot = system_stats->infos_posted;//per lp num info posted by lp
    system_stats->infos_posted/= n_cores;
    system_stats->infos_posted_attempt_tot = system_stats->infos_posted_attempt;//per lp num info posted by lp
    system_stats->infos_posted_attempt/= n_cores;
	#endif
	// Aggregate per LP
	for(i = 0; i < n_prc_tot; i++) {
		system_stats->events_total         += lp_stats[i].events_total;
		system_stats->events_straggler     += lp_stats[i].events_straggler;
		system_stats->events_committed     += lp_stats[i].events_committed;
		system_stats->events_reverse       += lp_stats[i].events_reverse;
		system_stats->events_undo          += lp_stats[i].events_undo;
		system_stats->events_stash         += lp_stats[i].events_stash;
		system_stats->events_silent        += lp_stats[i].events_silent;
		system_stats->events_silent_for_gvt        += lp_stats[i].events_silent_for_gvt;
		//system_stats->events_fetched       += lp_stats[i].events_fetched;
		system_stats->events_fetched_succ  += lp_stats[i].events_fetched_succ;
		//system_stats->events_fetched_unsucc+= lp_stats[i].events_fetched_unsucc;
		system_stats->events_enqueued       += lp_stats[i].events_enqueued;
		system_stats->events_anti          += lp_stats[i].events_anti;
		
		system_stats->counter_checkpoints  += lp_stats[i].counter_checkpoints;
		system_stats->counter_recoveries   += lp_stats[i].counter_recoveries;
		system_stats->counter_rollbacks    += lp_stats[i].counter_rollbacks;
		system_stats->counter_rollbacks_length    += lp_stats[i].counter_rollbacks_length;
		system_stats->counter_prune        += lp_stats[i].counter_prune;
		system_stats->counter_checkpoint_recalc += lp_stats[i].counter_checkpoint_recalc;
		system_stats->counter_safety_check += lp_stats[i].counter_safety_check;
		system_stats->counter_ongvt        += lp_stats[i].counter_ongvt;

		system_stats->clock_event          += lp_stats[i].clock_event;
		//system_stats->clock_fetch          += lp_stats[i].clock_fetch;
		system_stats->clock_fetch_succ     += lp_stats[i].clock_fetch_succ;
		//system_stats->clock_fetch_unsucc   += lp_stats[i].clock_fetch_unsucc;
		system_stats->clock_undo_event     += lp_stats[i].clock_undo_event;
		system_stats->clock_enqueue        += lp_stats[i].clock_enqueue;
		system_stats->clock_dequeue        += lp_stats[i].clock_dequeue;
		system_stats->clock_deq_lp         += lp_stats[i].clock_deq_lp;
		system_stats->clock_loop           += lp_stats[i].clock_loop;
		system_stats->clock_delete         += lp_stats[i].clock_delete;
		system_stats->clock_prune          += lp_stats[i].clock_prune;
		system_stats->clock_safety_check   += lp_stats[i].clock_safety_check;
		system_stats->clock_silent         += lp_stats[i].clock_silent;
		system_stats->clock_recovery       += lp_stats[i].clock_recovery;
		system_stats->clock_checkpoint     += lp_stats[i].clock_checkpoint;
		system_stats->clock_rollback       += lp_stats[i].clock_rollback;

		system_stats->mem_checkpoint       += lp_stats[i].mem_checkpoint;
		system_stats->checkpoint_period    += lp_stats[i].checkpoint_period;
		
		system_stats->total_frames         += LPS[i]->num_executed_frames;

	#if POSTING==1 && REPORT==1
    	system_stats->event_not_flushed  += lp_stats[i].event_not_flushed;//per lp event that lp father doesn't flush
    	system_stats->infos_posted_useful += lp_stats[i].infos_posted_useful;//per lp num info useful for lp
    	system_stats->sync_check_silent += lp_stats[i].sync_check_silent;//per lp num sync_check in past maded by lp
    	system_stats->sync_check_forward += lp_stats[i].sync_check_forward;//per lp num sync_check in future maded by lp
    	system_stats->sync_check_useful += lp_stats[i].sync_check_useful;//per lp num sync_check useful maded by lp
    	
    #endif
    	system_stats->events_exec_and_committed += lp_stats[i].events_exec_and_committed;
    	system_stats->clock_forward_exec += lp_stats[i].clock_forward_exec;

    	#if POSTING==1 || IPI_SUPPORT==1 && REPORT==1
    	system_stats->event_forward_interrupted += lp_stats[i].event_forward_interrupted;
    	system_stats->event_silent_interrupted += lp_stats[i].event_silent_interrupted;
    	system_stats->clock_exec_evt_inter_forward_exec += lp_stats[i].clock_exec_evt_inter_forward_exec;//per lp num clock cycles between start of ProcessEvent (with event in future) and relative interruption
    	system_stats->clock_exec_evt_inter_silent_exec += lp_stats[i].clock_exec_evt_inter_silent_exec;//per lp num clock cycles between start of ProcessEvent (with event in past) and relative interruption
    	#endif
	}
	
	

	system_stats->clock_safe          = system_stats->clock_event - system_stats->clock_silent;
	system_stats->clock_fetch         = system_stats->clock_fetch_succ + system_stats->clock_fetch_unsucc;
	system_stats->events_fetched      = system_stats->events_fetched_succ + system_stats->events_fetched_unsucc;
	
	system_stats->clock_event_tot     = system_stats->clock_event;
	system_stats->clock_safe_tot     = system_stats->clock_safe;
	
	system_stats->clock_event        /= system_stats->events_total;
	system_stats->clock_fetch        /= system_stats->events_fetched;
	system_stats->clock_fetch_succ   /= system_stats->events_fetched_succ;
	system_stats->clock_fetch_unsucc /= system_stats->events_fetched_unsucc;
	system_stats->clock_undo_event   /= system_stats->events_undo;
	system_stats->clock_silent       /= system_stats->events_silent;
	system_stats->clock_safety_check /= system_stats->counter_safety_check;
	system_stats->clock_rollback     /= system_stats->counter_rollbacks;
	// TODO
	system_stats->counter_rollbacks_length    += lp_stats[i].counter_rollbacks_length;
	system_stats->clock_checkpoint   /= system_stats->counter_checkpoints;
	system_stats->clock_recovery     /= system_stats->counter_recoveries;

	system_stats->clock_safe         /= (system_stats->events_total - system_stats->events_silent);
	system_stats->clock_frame_tot     = system_stats->clock_safe * system_stats->total_frames;

	system_stats->events_get_next_fetch /= system_stats->events_fetched;	
	system_stats->clock_enqueue       /= system_stats->events_enqueued;
		

	system_stats->counter_checkpoint_recalc /= n_prc_tot;

	system_stats->mem_checkpoint /= system_stats->counter_checkpoints;
	system_stats->checkpoint_period /= n_prc_tot;

	#if POSTING==1 && REPORT==1
	//calculate in gather_statistics()
    system_stats->event_not_flushed_tot = system_stats->event_not_flushed;//per lp event that lp father doesn't flush
    system_stats->event_not_flushed/= n_prc_tot;

    system_stats->infos_posted_useful_tot = system_stats->infos_posted_useful;//per lp num info useful for lp
    system_stats->infos_posted_useful/= n_prc_tot;

    system_stats->sync_check_silent_tot  =  system_stats->sync_check_silent;//per lp num sync_check in past maded by lp
    system_stats->sync_check_silent/= n_prc_tot;

    system_stats->sync_check_forward_tot  =  system_stats->sync_check_forward;//per lp num sync_check in future maded by lp
    system_stats->sync_check_forward/= n_prc_tot;
    #endif

    #if IPI_SUPPORT==1 || POSTING==1 && REPORT==1
    system_stats->sync_check_useful_tot  =  system_stats->sync_check_useful;//per lp num sync_check useful maded by lp
    system_stats->sync_check_useful/= n_prc_tot;

    system_stats->clock_exec_interruption_tot = system_stats->clock_exec_evt_inter_forward_exec+system_stats->clock_exec_evt_inter_silent_exec;
    system_stats->clock_exec_interruption = system_stats->clock_exec_interruption_tot /n_prc_tot;

    system_stats->clock_exec_evt_inter_forward_exec_tot  =  system_stats->clock_exec_evt_inter_forward_exec;//per lp num clock cycles between start of ProcessEvent (with event in future) and relative interruption
    system_stats->clock_exec_evt_inter_forward_exec/= n_prc_tot;

    system_stats->clock_exec_evt_inter_silent_exec_tot  =  system_stats->clock_exec_evt_inter_silent_exec;//per lp num clock cycles between start of ProcessEvent (with event in past) and relative interruption
    system_stats->clock_exec_evt_inter_silent_exec /= n_prc_tot;

    system_stats->event_interrupted_tot = system_stats->event_forward_interrupted + system_stats->event_silent_interrupted;
    system_stats->event_interrupted = system_stats->event_interrupted_tot/n_prc_tot;
    system_stats->event_forward_interrupted_tot= system_stats->event_forward_interrupted;
     system_stats->event_forward_interrupted /= n_prc_tot;
     system_stats->event_silent_interrupted_tot= system_stats->event_silent_interrupted;
     system_stats->event_silent_interrupted /= n_prc_tot;
    #endif

    system_stats->events_exec_and_committed_tot= system_stats->events_exec_and_committed;
    system_stats->events_exec_and_committed /= n_prc_tot;

    system_stats->clock_forward_exec_tot= system_stats->clock_forward_exec;
    system_stats->clock_forward_exec /= n_prc_tot;
    system_stats->clock_forward_exec_per_event = system_stats->clock_forward_exec_tot /(system_stats->events_total - system_stats->events_silent);
}

static void _print_statistics(struct stats_t *stats) {

	printf(COLOR_CYAN);

	printf("Total events....................................: %12llu\n", (unsigned long long)stats->events_total);
	printf("Committed events................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_committed, percentage(stats->events_committed, stats->events_total));
	printf("Events executed and committed tot...............: %12lu\n",
		(unsigned long)stats->events_exec_and_committed_tot);
	printf("Events executed and committed per LP............: %12.2f\n",
		stats->events_exec_and_committed);
	printf("Straggler events................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_straggler, percentage(stats->events_straggler, stats->events_total));
	printf("Anti events.....................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_anti, percentage(stats->events_anti, stats->events_total));
	printf("Useless events..................................: %12llu (%4.2f%%)\n", 
		(unsigned long long)(stats->events_total - stats->events_silent - stats->total_frames+n_prc_tot),percentage((stats->events_total - stats->events_silent - stats->total_frames+n_prc_tot), stats->events_total));
#if REVERSIBLE==1
	printf("Reversible events...............................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_reverse, percentage(stats->events_reverse, stats->events_total));
	printf("Stashed events..................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_stash, percentage(stats->events_stash, stats->events_total));
#endif
	printf("Silent events...................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_silent, percentage(stats->events_silent, stats->events_total));
	printf("Silent events for GVT...........................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_silent_for_gvt, percentage(stats->events_silent_for_gvt, stats->events_silent));
	
	printf("Forward execution Time tot......................: %12lu clocks\n",
		(unsigned long)stats->clock_forward_exec_tot);
	printf("Forward execution Time per LP...................: %12.2f clocks\n",
		stats->clock_forward_exec);
	printf("Forward execution Time per event................: %12.2f clocks\n",
		stats->clock_forward_exec_per_event);

	printf("\n");
	
	printf("Flushed events..................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_enqueued, percentage(stats->events_enqueued, stats->events_total));
	printf("Fetched operations..............................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched, percentage(stats->events_fetched_succ, stats->events_total));
	printf("   Fetch succeed................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched_succ, percentage(stats->events_fetched_succ, stats->events_fetched));
	printf("   Fetch failed.................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched_unsucc, percentage(stats->events_fetched_unsucc, stats->events_fetched));
	printf("   Avg node traversed during fetch..............: %12.2f\n", stats->events_get_next_fetch);
	
	printf("\n");
	

	printf("Average time to process any event...............: %12.2f clocks\n", stats->clock_event);
	printf("   Average time spent in standard execution.....: %12.2f clocks\n", stats->clock_safe);
	printf("   Average time spent in silent execution.......: %12.2f clocks (%4.2f%%)\n", stats->clock_silent, 0.0);
	printf("Average time spent to fetch.....................: %12.2f clocks (%4.2f%%)\n", stats->clock_fetch, 0.0);
	printf("   Average time in successfull fetch............: %12.2f clocks (%4.2f%%)\n", stats->clock_fetch_succ, 0.0);
	printf("   Average time in unsuccessfull fetch..........: %12.2f clocks (%4.2f%%)\n", stats->clock_fetch_unsucc, 0.0);
	printf("Average time spent to Enqueue...................: %12.2f clocks (%4.2f%%)\n", stats->clock_enqueue, 0.0);
	
	
	
	printf("\n");

	printf("Save Checkpoint operations......................: %12llu\n", (unsigned long long)stats->counter_checkpoints);
	printf("Restore Checkpoint operations...................: %12llu\n", (unsigned long long)stats->counter_recoveries);
	printf("Rollback operations.............................: %12llu\n", (unsigned long long)stats->counter_rollbacks);
	printf("AVG Rollbacked Events...........................: %12.2f\n", stats->counter_rollbacks_length/stats->counter_rollbacks);
	printf("CheckOnGVT invocations..........................: %12llu\n", (unsigned long long)stats->counter_ongvt);
	
	printf("\n");

	printf("Save Checkpoint time............................: %12.2f clocks (%4.2f%%)\n",
		stats->clock_checkpoint, percentage(stats->clock_checkpoint, stats->clock_rollback));
	printf("Restore Checkpoint time.........................: %12.2f clocks (%4.2f%%)\n",
		stats->clock_recovery, percentage(stats->clock_recovery, stats->clock_rollback));
	printf("Rollback time...................................: %12.2f clocks\n", stats->clock_rollback);
	printf("Checkpoint mem..................................: %12u B\n", (unsigned int)stats->mem_checkpoint);
	printf("Checkpoint recalculations.......................: %12llu\n", (unsigned long long)stats->counter_checkpoint_recalc);
	printf("Checkpoint period...............................: %12llu\n", (unsigned long long)stats->checkpoint_period);
	
	printf("\n\n");

	#if IPI_SUPPORT==1 && REPORT==1
	printf("IPI sent tot....................................: %12lu\n",
		(unsigned long)stats->ipi_sended_tot);
	printf("IPI sent per thread.............................: %12.2f\n",
		stats->ipi_sended);
	printf("IPI handled in trampoline tot...................: %12lu\n",
		(unsigned long)stats->ipi_trampoline_received_tot);
	printf("IPI handled in trampoline per thread............: %12.2f\n",
		stats->ipi_trampoline_received);
	printf("IPI received tot................................: %12lu\n",
		(unsigned long)stats->ipi_received_tot);
	printf("IPI received per thread.........................: %12.2f\n",
		stats->ipi_received);
	printf("IPI sent sycall clocks....................................: %12lu\n",
		(unsigned long)stats->clock_exec_ipi_syscall_tot);
	printf("\n\n");
	#endif

	#if POSTING==1 && REPORT==1
	printf("Event not flushed tot...........................: %12lu\n",
		(unsigned long)stats->event_not_flushed_tot);
	printf("Event not flushed per LP........................: %12.2f\n",
		stats->event_not_flushed);

    printf("\n");

    printf("Num attempts Info posted tot....................: %12lu\n",
		(unsigned long)stats->infos_posted_attempt_tot);
	printf("Num attempts Info posted per thread.............: %12.2f\n",
		stats->infos_posted_attempt);

	printf("Info posted tot.................................: %12lu\n",
		(unsigned long)stats->infos_posted_tot);
	printf("Info posted per thread..........................: %12.2f\n",
		stats->infos_posted);

	printf("Info posted useful tot..........................: %12lu\n",
		(unsigned long)stats->infos_posted_useful_tot);
	printf("Info posted useful per LP.......................: %12.2f\n",
		stats->infos_posted_useful);

	printf("\n");

	printf("Sync check Silent Execution tot.................: %12lu\n",
		(unsigned long)stats->sync_check_silent_tot);
	printf("Sync check Silent Execution per LP..............: %12.2f\n",
		stats->sync_check_silent);

	printf("Sync check Forward Execution tot................: %12lu\n",
		(unsigned long)stats->sync_check_forward_tot);
	printf("Sync check Forward Execution per LP.............: %12.2f\n",
		stats->sync_check_forward);
	#endif

	#if IPI_SUPPORT==1 || POSTING==1 && REPORT==1
	printf("Sync check useful tot...........................: %12lu\n",
		(unsigned long)stats->sync_check_useful_tot);
	printf("Sync check useful per LP........................: %12.2f\n",
		stats->sync_check_useful);

	printf("\n");

	printf("Events forward interrupted tot..................: %12lu\n",
		(unsigned long)stats->event_forward_interrupted_tot);
	printf("Events forward interrupted per LP...............: %12.2f\n",
		stats->event_forward_interrupted);

	printf("Events silent interrupted tot...................: %12lu\n",
		(unsigned long)stats->event_silent_interrupted_tot);
	printf("Events silent interrupted per LP................: %12.2f\n",
		stats->event_silent_interrupted);

	printf("Events interrupted tot..........................: %12lu\n",
		(unsigned long)stats->event_interrupted_tot);
	printf("Events interrupted per LP.......................: %12.2f\n",
		stats->event_interrupted);

	printf("Evt_interrupted_forward_exec Time tot...........: %12lu clocks\n",
		(unsigned long)stats->clock_exec_evt_inter_forward_exec_tot);
	printf("Evt_interrupted_forward_exec Time per LP........: %12.2f clocks\n",
		stats->clock_exec_evt_inter_forward_exec);

	printf("Evt_interrupted_silent_exec Time tot............: %12lu clocks\n",
		(unsigned long)stats->clock_exec_evt_inter_silent_exec_tot);
	printf("Evt_interrupted_silent_exec Time per LP.........: %12.2f clocks\n",
		stats->clock_exec_evt_inter_silent_exec);

	printf("Evt_interrupted Time tot........................: %12lu clocks\n",
		(unsigned long)stats->clock_exec_interruption_tot);
	printf("Evt_interrupted_Time per LP.....................: %12.2f clocks\n",
		stats->clock_exec_interruption);

	printf("\n\n");
	#endif

	printf("Total Clock.....................................: %12llu clocks\n", 
		(unsigned long long)stats->clock_loop_tot);
	printf("Event Processing................................: %12llu clocks (%4.2f%%)\n", 
		(unsigned long long)stats->clock_event_tot, percentage(stats->clock_event_tot,stats->clock_loop_tot));
	printf("Safe Processing.................................: %12llu clocks (%4.2f%%)\n", 
		(unsigned long long)stats->clock_safe_tot, percentage(stats->clock_safe_tot,stats->clock_loop_tot));
	printf("Frame Processing................................: %12llu clocks (%4.2f%%)\n", 
		(unsigned long long)stats->clock_frame_tot, percentage(stats->clock_frame_tot,stats->clock_loop_tot));

	printf(COLOR_RESET"\n");
}

void print_statistics() {
	//unsigned int i;
	//FILE *stat_file;

	gather_statistics();

	printf("Simulation final report:\n");
	printf("Thread: %u\tLP: %u\n", n_cores, n_prc_tot);
	//printf("Lookahead: %d", 0);
	printf("\n");

/*
	printf("\n");
	printf("===== LPs' STATISTICS =====\n");
	for (int i = 0; i < n_prc_tot; i++)	{
		printf("\n---------- LP%u ----------\n", i);
		_print_statistics(lp_stats+i);
		printf("\n", i);
	}
*/

	printf("===== SYSTEM-WIDE STATISTICS =====\n");
	_print_statistics(system_stats);
}
