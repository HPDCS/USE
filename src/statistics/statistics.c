#include <stdio.h>

#include <statistics.h>
#include <core.h>
#include <lp/lp.h>

#include <utils/prints.h>

#include <glo_alloc.h>
#include <thr_alloc.h>
#include <lpm_alloc.h>

extern double delta_count;
extern unsigned int reverse_execution_threshold_main;

struct stats_t *thread_stats;
struct stats_t *lp_stats;
struct stats_t *system_stats;

void statistics_init() {
	if(thr_memalign_alloc((void**)&thread_stats, 64, pdes_config.ncores * sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n"); // LCOV_EXCL_LINE
	}
	if(thread_stats == NULL) {
		printf("Unable to allocate thread statistics vector\n"); // LCOV_EXCL_LINE
		abort();																								 // LCOV_EXCL_LINE
	}

	if(lpm_memalign_alloc((void**)&lp_stats, 64, pdes_config.nprocesses * sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n"); // LCOV_EXCL_LINE
	}
	if(lp_stats == NULL) {
		printf("Unable to allocate LP statistics vector\n"); // LCOV_EXCL_LINE
		abort();																						 // LCOV_EXCL_LINE
	}

	if(glo_memalign_alloc((void**)&system_stats, 64, sizeof(struct stats_t)) < 0) {
		printf("memalign failed\n"); // LCOV_EXCL_LINE
	}
	if(lp_stats == NULL) {
		printf("Unable to allocate LP statistics vector\n"); // LCOV_EXCL_LINE
		abort();																						 // LCOV_EXCL_LINE
	}

	memset(thread_stats, 0, pdes_config.ncores * sizeof(struct stats_t));
	memset(lp_stats, 0, pdes_config.nprocesses * sizeof(struct stats_t));
	memset(system_stats, 0, sizeof(struct stats_t));
}

void statistics_fini() {
	thr_free(thread_stats);
	lpm_free(lp_stats);
	glo_free(system_stats);
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

		case STAT_EVENT_SILENT_FOR_GVT:
			stats[idx].events_silent_for_gvt += value;
			break;

		case STAT_EVENT_SILENT:
			stats[idx].events_silent += value;
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
			break;

		case STAT_RECOVERY:
			stats[idx].counter_recoveries += value;
			break;

		// ========= TIME STATS ========= //
		case STAT_CLOCK_EVENT:
			stats[idx].clock_event += value;
			break;

		case STAT_CLOCK_ENQUEUE:
			stats[idx].clock_enqueue += value;
			break;

		case STAT_CLOCK_LOOP:
			stats[idx].clock_loop += value;
			break;

		case STAT_CLOCK_PRUNE:
			stats[idx].clock_prune += value;
			break;

		case STAT_CLOCK_SILENT:
			stats[idx].clock_silent += value;
			break;

		case STAT_CLOCK_ROLLBACK:
			stats[idx].clock_rollback += value;
			break;

		case STAT_CKPT_TIME:
			stats[idx].clock_checkpoint += value;
			break;

		case STAT_RECOVERY_TIME:
			stats[idx].clock_recovery += value;
			break;

		case STAT_CLOCK_FETCH_SUCC:
			stats[idx].clock_fetch_succ += value;
			break;

		case STAT_CLOCK_FETCH_UNSUCC:
			stats[idx].clock_fetch_unsucc += value;
			break;

		case STAT_EVT_FROM_GLOBAL_FETCH:
			stats[idx].events_from_global_index += value;
			break;
		case STAT_EVT_FROM_LOCAL_FETCH:
			stats[idx].events_from_local_index += value;
			break;
		case STAT_INIT_CLOCKS:
            stats[idx].clock_init += value;
            break;
		default:																					// LCOV_EXCL_LINE
			printf("Unrecognized stat type (%d)\n", type);  // LCOV_EXCL_LINE
	}
}

inline void statistics_post_lp_data(unsigned int lp_idx, int type, stat64_t value) {
	assert(lp_idx < pdes_config.nprocesses);
	statistics_post_data(lp_stats, lp_idx, type, value);
}

inline void statistics_post_th_data(unsigned int tid, int type, stat64_t value) {
	assert(tid < pdes_config.ncores);
	statistics_post_data(thread_stats, tid, type, value);
}


void gather_statistics() {
	unsigned int i;

	// Aggregate per thread
	for(i = 0; i < pdes_config.ncores; i++) {
		system_stats->events_committed     += thread_stats[i].events_committed;
		
		system_stats->counter_prune        += thread_stats[i].counter_prune;
		//system_stats->counter_ongvt        += thread_stats[i].counter_ongvt;

		//system_stats->clock_enqueue        += thread_stats[i].clock_enqueue;
		//system_stats->clock_dequeue        += thread_stats[i].clock_dequeue;
		//system_stats->clock_deq_lp         += thread_stats[i].clock_deq_lp;
		system_stats->clock_loop           += thread_stats[i].clock_loop;
		system_stats->clock_prune          += thread_stats[i].clock_prune;
		system_stats->clock_init           += thread_stats[i].clock_init;
		//system_stats->clock_loop           += thread_stats[i].clock_loop;
		
		system_stats->events_fetched       += thread_stats[i].events_fetched;
		system_stats->events_fetched_unsucc+= thread_stats[i].events_fetched_unsucc;
		system_stats->clock_fetch_unsucc   += thread_stats[i].clock_fetch_unsucc;
		system_stats->events_get_next_fetch+= thread_stats[i].events_get_next_fetch;
		system_stats->events_from_local_index += thread_stats[i].events_from_local_index;
		system_stats->events_from_global_index += thread_stats[i].events_from_global_index;
	}

	system_stats->clock_prune /= system_stats->counter_prune;
	system_stats->clock_loop_tot = system_stats->clock_loop;
	system_stats->clock_loop /= pdes_config.ncores;
	
	// Aggregate per LP
	for(i = 0; i < pdes_config.nprocesses; i++) {
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
	}
	
	system_stats->clock_safe          = system_stats->clock_event - system_stats->clock_silent;
	system_stats->clock_fetch         = system_stats->clock_fetch_succ + system_stats->clock_fetch_unsucc;
	system_stats->events_fetched      = system_stats->events_fetched_succ + system_stats->events_fetched_unsucc;
	
	system_stats->clock_event_tot     = system_stats->clock_event;
	system_stats->clock_safe_tot      = system_stats->clock_safe;
	
	system_stats->clock_event        /= system_stats->events_total;
	system_stats->clock_fetch        /= system_stats->events_fetched;
	system_stats->clock_fetch_succ   /= system_stats->events_fetched_succ;
	system_stats->clock_fetch_unsucc /= system_stats->events_fetched_unsucc;
	system_stats->clock_undo_event   /= system_stats->events_undo;
	system_stats->clock_silent       /= system_stats->events_silent;
	system_stats->clock_safety_check /= system_stats->counter_safety_check;
	system_stats->clock_rollback     /= system_stats->counter_rollbacks;
	system_stats->counter_rollbacks_length    += lp_stats[i].counter_rollbacks_length;
	system_stats->clock_checkpoint   /= system_stats->counter_checkpoints;
	system_stats->clock_recovery     /= system_stats->counter_recoveries;

	system_stats->clock_safe         /= (system_stats->events_total - system_stats->events_silent);
	system_stats->clock_frame_tot     = system_stats->clock_safe * system_stats->total_frames;


	system_stats->events_get_next_fetch /= system_stats->events_fetched;	
	system_stats->clock_enqueue       /= system_stats->events_enqueued;
		

	system_stats->counter_checkpoint_recalc /= pdes_config.nprocesses;

	system_stats->mem_checkpoint /= system_stats->counter_checkpoints;
	system_stats->checkpoint_period /= pdes_config.nprocesses;
}

static void _print_statistics(struct stats_t *stats) {

  #if PRINT_SCREEN == 1
	printf(COLOR_CYAN);
  #endif
	printf("Total events....................................: %12llu\n", (unsigned long long)stats->events_total);
	printf("Committed events................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_committed, percentage(stats->events_committed, stats->events_total));
	printf("Straggler events................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_straggler, percentage(stats->events_straggler, stats->events_total));
	printf("Anti events.....................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_anti, percentage(stats->events_anti, stats->events_total));
	printf("Useless events..................................: %12llu (%4.2f%%)\n", 
		(unsigned long long)(stats->events_total - stats->events_silent - stats->total_frames+pdes_config.nprocesses),percentage((stats->events_total - stats->events_silent - stats->total_frames+pdes_config.nprocesses), stats->events_total));
#if REVERSIBLE==1
	printf("Reversible events...............................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_reverse, percentage(stats->events_reverse, stats->events_total));
	printf("Stashed events..................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_stash, percentage(stats->events_stash, stats->events_total));
#endif
	printf("Silent events...................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_silent, percentage(stats->events_silent, stats->events_total));
	printf("Silent events for GVT...........................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_silent_for_gvt, percentage(stats->events_silent_for_gvt, stats->events_total));
    printf("Flushed events..................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_enqueued, percentage(stats->events_enqueued, stats->events_total));

	printf("Average time to process any event...............: %12.2f clocks\n", stats->clock_event);
	printf("   Average time spent in standard execution.....: %12.2f clocks\n", stats->clock_safe);
	printf("   Average time spent in silent execution.......: %12.2f clocks\n", stats->clock_silent);

    printf("\n");
	
    printf("Fetched operations..............................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched, percentage(stats->events_fetched, stats->events_fetched));
    printf("   Fetch succeed................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched_succ, percentage(stats->events_fetched_succ, stats->events_fetched));
	printf("   Fetch failed.................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_fetched_unsucc, percentage(stats->events_fetched_unsucc, stats->events_fetched));
	printf("   Avg node traversed during fetch..............: %12.2f\n", stats->events_get_next_fetch);
	printf("   Fetch from global index......................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_from_global_index, percentage(stats->events_from_global_index, stats->events_fetched));
	printf("   Fetch from local index.......................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_from_local_index, percentage(stats->events_from_local_index, stats->events_fetched));
	
	printf("Average time spent to fetch.....................: %12.2f clocks\n", stats->clock_fetch);
	printf("   Average time in successfull fetch............: %12.2f clocks\n", stats->clock_fetch_succ);
	printf("   Average time in unsuccessfull fetch..........: %12.2f clocks\n", stats->clock_fetch_unsucc);
	printf("Average time spent to Enqueue...................: %12.2f clocks\n", stats->clock_enqueue);
	
	
	
	printf("\n");

	printf("Save Checkpoint operations......................: %12llu\n", (unsigned long long)stats->counter_checkpoints);
	printf("Restore Checkpoint operations...................: %12llu\n", (unsigned long long)stats->counter_recoveries);
	printf("Rollback operations.............................: %12llu\n", (unsigned long long)stats->counter_rollbacks);
	printf("AVG Rollbacked Events...........................: %12.2f\n", stats->counter_rollbacks_length/stats->counter_rollbacks);
	printf("CheckOnGVT invocations..........................: %12llu\n", (unsigned long long)stats->counter_ongvt);
	
	printf("\n");

	printf("AVG Rollback time...............................: %12.2f clocks (%4.2f%%)\n", 
        stats->clock_rollback, percentage(stats->clock_rollback,stats->clock_rollback));
	printf("Save Checkpoint time............................: %12.2f clocks (%4.2f%%)\n",
		stats->clock_checkpoint, percentage(stats->clock_checkpoint, stats->clock_rollback));
	printf("Load Checkpoint time............................: %12.2f clocks (%4.2f%%)\n",
		stats->clock_recovery, percentage(stats->clock_recovery, stats->clock_rollback));

	printf("\n");


	printf("Checkpoint mem..................................: %12u B\n", (unsigned int)stats->mem_checkpoint);
	printf("Checkpoint recalculations.......................: %12llu\n", (unsigned long long)stats->counter_checkpoint_recalc);
	printf("Checkpoint period...............................: %12llu\n", (unsigned long long)stats->checkpoint_period);

	printf("\n");
	printf("Prune counts....................................: %12llu\n", (unsigned long long)stats->counter_prune);
	printf("AVG Prune clocks................................: %12llu\n", (unsigned long long)stats->clock_prune);

	
	printf("\n\n");
	
	
	printf("Total Clocks..............................: %14llu clocks\n", 
		(unsigned long long)stats->clock_loop_tot);
	printf("Init  Clocks..............................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)stats->clock_init, percentage(stats->clock_init,stats->clock_loop_tot));
	printf("Safe   Event Clocks.......................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)stats->clock_safe_tot, percentage(stats->clock_safe_tot,stats->clock_loop_tot));
	printf("Silent Event Clocks.......................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->clock_event_tot-stats->clock_safe_tot), percentage((stats->clock_event_tot-stats->clock_safe_tot),stats->clock_loop_tot));
//	printf("Frame Clocks..............................: %14llu clocks (%4.2f%%)\n", 
//		(unsigned long long)stats->clock_frame_tot, percentage(stats->clock_frame_tot,stats->clock_loop_tot));
	printf("Prune Clocks..............................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->counter_prune*stats->clock_prune), percentage(stats->counter_prune*stats->clock_prune,stats->clock_loop_tot));
	printf("Fetch Clocks..............................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->events_fetched*stats->clock_fetch), percentage(stats->events_fetched*stats->clock_fetch,stats->clock_loop_tot));
	printf("Flush Clocks..............................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->events_enqueued*stats->clock_enqueue), percentage(stats->events_enqueued*stats->clock_enqueue,stats->clock_loop_tot));
	printf("Save  Chkp Clocks.........................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->counter_checkpoints*stats->clock_checkpoint), percentage(stats->counter_checkpoints*stats->clock_checkpoint,stats->clock_loop_tot));
	printf("Load  Chkp Clocks.........................: %14llu clocks (%4.2f%%)\n", 
		(unsigned long long)(stats->counter_rollbacks*stats->clock_rollback - (stats->clock_event_tot-stats->clock_safe_tot)), percentage(stats->counter_rollbacks*stats->clock_rollback - (stats->clock_event_tot-stats->clock_safe_tot),stats->clock_loop_tot));

	printf(COLOR_RESET"\n");
}

void print_statistics() {
	//unsigned int i;
	//FILE *stat_file;

	gather_statistics();

	printf("Simulation final report:\n");
	printf("Thread: %u\tLP: %u\n", pdes_config.ncores, pdes_config.nprocesses);
	//printf("Lookahead: %d", 0);
	printf("\n");

/*
	printf("\n");
	printf("===== LPs' STATISTICS =====\n");
	for (int i = 0; i < pdes_config.nprocesses; i++)	{
		printf("\n---------- LP%u ----------\n", i);
		_print_statistics(lp_stats+i);
		printf("\n", i);
	}
*/

	printf("===== SYSTEM-WIDE STATISTICS =====\n");
	_print_statistics(system_stats);
}
