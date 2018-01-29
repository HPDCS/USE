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

		case STAT_CLOCK_FETCH:
			stats[idx].clock_fetch += value;
			break;

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
	
	// Aggregate per LP
	for(i = 0; i < n_prc_tot; i++) {
		system_stats->events_total         += lp_stats[i].events_total;
		system_stats->events_straggler     += lp_stats[i].events_straggler;
		system_stats->events_committed     += lp_stats[i].events_committed;
		system_stats->events_reverse       += lp_stats[i].events_reverse;
		system_stats->events_undo          += lp_stats[i].events_undo;
		system_stats->events_stash         += lp_stats[i].events_stash;
		system_stats->events_silent        += lp_stats[i].events_silent;
		system_stats->events_fetched       += lp_stats[i].events_fetched;
		system_stats->events_enqueued       += lp_stats[i].events_enqueued;
		system_stats->events_anti          += lp_stats[i].events_anti;
		
		system_stats->counter_checkpoints  += lp_stats[i].counter_checkpoints;
		system_stats->counter_recoveries   += lp_stats[i].counter_recoveries;
		system_stats->counter_rollbacks    += lp_stats[i].counter_rollbacks;
		system_stats->counter_prune        += lp_stats[i].counter_prune;
		system_stats->counter_checkpoint_recalc += lp_stats[i].counter_checkpoint_recalc;
		system_stats->counter_safety_check += lp_stats[i].counter_safety_check;
		system_stats->counter_ongvt        += lp_stats[i].counter_ongvt;

		system_stats->clock_event          += lp_stats[i].clock_event;
		system_stats->clock_fetch          += lp_stats[i].clock_fetch;
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

	system_stats->clock_event        /= system_stats->events_total;
	system_stats->clock_fetch        /= system_stats->events_fetched;
	system_stats->clock_undo_event   /= system_stats->events_undo;
	system_stats->clock_silent       /= system_stats->events_silent;
	system_stats->clock_safety_check /= system_stats->counter_safety_check;
	system_stats->clock_rollback     /= system_stats->counter_rollbacks;
	system_stats->clock_checkpoint   /= system_stats->counter_checkpoints;
	system_stats->clock_recovery     /= system_stats->counter_recoveries;

	system_stats->counter_checkpoint_recalc /= n_prc_tot;

	system_stats->mem_checkpoint /= system_stats->counter_checkpoints;
	system_stats->checkpoint_period /= n_prc_tot;
}

static void _print_statistics(struct stats_t *stats) {

	printf(COLOR_CYAN);

	printf("Total events....................................: %.2f\n", stats->events_total);
	printf("Committed events................................: %.2f (%.2f%%)\n",
		stats->events_committed, percentage(stats->events_committed, stats->events_total));
	printf("Straggler events................................: %.2f (%.2f%%)\n",
		stats->events_straggler, percentage(stats->events_straggler, stats->events_total));
	printf("Anti events.....................................: %.2f (%.2f%%)\n",
		stats->events_anti, percentage(stats->events_anti, stats->events_total));
#if REVERSIBLE==1
	printf("Reversible events...............................: %.2f (%.2f%%)\n",
		stats->events_reverse, percentage(stats->events_reverse, stats->events_total));
	printf("Stashed events..................................: %.2f (%.2f%%)\n",
		stats->events_stash, percentage(stats->events_stash, stats->events_total));
#endif
	printf("Reprocessed events..............................: %.2f (%.2f%%)\n",
		stats->events_silent, percentage(stats->events_silent, stats->events_total));
	printf("Flushed events..................................: %.2f (%.2f%%)\n",
		stats->events_enqueued, percentage(stats->events_enqueued, stats->events_total));
	printf("Fetched events..................................: %.2f (%.2f%%)\n",
		stats->events_fetched, percentage(stats->events_fetched, stats->events_total));
	printf("OnGVT invocations...............................: %.2f\n", stats->counter_ongvt);
	
	printf("\n");
	
	printf("Useless work....................................: %.2f\n", stats->events_total - stats->events_silent - stats->total_frames);

	printf("\n");

	printf("Average time to process an event................: %.2f clocks\n", stats->clock_event);
	printf("Average time spent to fetch.....................: %.2f clocks (%.2f%%)\n", stats->clock_fetch, 0.0);
	printf("Average time spent in silent execution..........: %.2f clocks (%.2f%%)\n", stats->clock_silent, 0.0);
	
	printf("\n");

	printf("Checkpoint operations...........................: %.2f\n", stats->counter_checkpoints);
	printf("Restore operations..............................: %.2f\n", stats->counter_recoveries);
	printf("Rollback operations.............................: %.2f\n", stats->counter_rollbacks);

	printf("\n");

	printf("Rollback time...................................: %.2f clocks\n", stats->clock_rollback);
	printf("Restore time....................................: %.2f clocks (%.2f%%)\n",
		stats->clock_recovery, percentage(stats->clock_recovery, stats->clock_rollback));
	printf("Checkpoint time.................................: %.2f clocks (%.2f%%)\n",
		stats->clock_checkpoint, percentage(stats->clock_checkpoint, stats->clock_rollback));
	printf("Checkpoint mem..................................: %.2f Bytes\n", stats->mem_checkpoint);
	printf("Checkpoint recalculations.......................: %.2f\n", stats->counter_checkpoint_recalc);
	printf("Checkpoint period...............................: %.2f\n", stats->checkpoint_period);

	printf(COLOR_RESET);
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
