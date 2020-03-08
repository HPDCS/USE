#include <stdio.h>

#include <statistics.h>
#include <core.h>
#include <hpdcs_utils.h>
#include <simtypes.h>
#include <unistd.h>

#if IPI_DECISION_MODEL==1
#include <ipi_decision_model_stats.h>
#endif

#include <malloc.h> //useful to print allocation info related to malloc functions

#include <sys/types.h>
#include <unistd.h>
#include <memory_limit.h>
//Variabili da tunare durante l'esecuzione per throttling e threshold
extern double delta_count;
extern unsigned int reverse_execution_threshold;

struct stats_t *thread_stats;
struct stats_t *lp_stats;
struct stats_t *system_stats;

volatile double simduration;

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

	#if IPI_DECISION_MODEL==1
	init_ipi_decision_model_stats();//allocation of lp_stats structs for all LPs
	#endif
}

void statistics_fini() {
	free(thread_stats);
	free(lp_stats);
	free(system_stats);

	#if IPI_DECISION_MODEL==1
    fini_ipi_decision_model_stats();
    #endif
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












		//new_statistics added
		#if STATISTICS_ADDED==1 && REPORT==1
		case STAT_EVENTS_EXEC_AND_COMMITED_LP:
			stats[idx].events_exec_and_committed_lp += value;//per LP
			break;
		#endif

		#if POSTING==1 && REPORT==1

		case STAT_INFOS_POSTED_TID:
			stats[idx].infos_posted_tid += value;
			break;
		case STAT_INFOS_POSTED_ATTEMPT_TID:
			stats[idx].infos_posted_attempt_tid += value;
			break;
		case STAT_INFOS_POSTED_USEFUL_TID:
			stats[idx].infos_posted_useful_tid += value;
			break;
		#endif


		#if HANDLE_INTERRUPT==1 && REPORT==1
		case STAT_EVENT_NOT_FLUSHED_LP:
			stats[idx].event_not_flushed_lp += value;
			break;

		case STAT_CLOCK_EXPOSITION_FORWARD_TOT_LP:
			stats[idx].clock_exposition_forward_tot_lp += value;
			break;
		case STAT_CLOCK_EXPOSITION_SILENT_TOT_LP:
			stats[idx].clock_exposition_silent_tot_lp += value;
			break;

		case STAT_EVENT_EXPOSITION_FORWARD_LP:
			stats[idx].event_exposition_forward_lp += value;
			break;
		case STAT_EVENT_EXPOSITION_SILENT_LP:
			stats[idx].event_exposition_silent_lp += value;
			break;

		case STAT_EVENT_RESUME_ROLLBACK_LP:
			stats[idx].event_resume_rollback_tot_lp += value;
			break;

		case STAT_COUNT_RESUME_ROLLBACK_LP:
			stats[idx].count_resume_rollback_tot_lp +=value;
			break;

		case STAT_CLOCK_RESUME_ROLLBACK_LP:
			stats[idx].clock_resume_rollback_tot_lp += value;
			break;
		#endif

		#if HANDLE_INTERRUPT_WITH_CHECK==1 && REPORT==1
		case STAT_SYNC_CHECK_SILENT_LP:
			stats[idx].sync_check_silent_lp += value;
			break;
		case STAT_SYNC_CHECK_FORWARD_LP:
			stats[idx].sync_check_forward_lp += value;
			break;
		case STAT_SYNC_CHECK_USEFUL_LP:
			stats[idx].sync_check_useful_lp += value;
			break;
		#endif

		#if IPI_SUPPORT==1 && REPORT==1

		case STAT_IPI_SENDED_TID:
			stats[idx].ipi_sent_tid += value;
			break;
		case STAT_IPI_RECEIVED_TID:
			stats[idx].ipi_received_tid += value;
			break;
		case STAT_IPI_SYSCALL_TIME_TID:
			stats[idx].clock_exec_ipi_syscall_tid += value;
			break;
		#endif

		#if IPI_DECISION_MODEL==1 && REPORT==1
		case STAT_IPI_FILTERED_IN_DECISION_MODEL_TID:
			stats[idx].ipi_filtered_in_decision_model_tid += value;
			break;

		case STAT_EVENT_EXPOSITION_FORWARD_ASYNCH_INTERRUPTED_LP:
			stats[idx].event_exposition_forward_asynch_interrupted_lp += value;
			break;
		case STAT_EVENT_EXPOSITION_SILENT_ASYNCH_INTERRUPTED_LP:
			stats[idx].event_exposition_silent_asynch_interrupted_lp += value;
			break;

		case STAT_EVENT_EXPOSITION_FORWARD_SYNCH_INTERRUPTED_LP:
			stats[idx].event_exposition_forward_synch_interrupted_lp += value;
			break;
		case STAT_EVENT_EXPOSITION_SILENT_SYNCH_INTERRUPTED_LP:
			stats[idx].event_exposition_silent_synch_interrupted_lp += value;
			break;


		case STAT_CLOCK_EXPOSITION_FORWARD_ASYNCH_INTERRUPTED_LP:
			stats[idx].clock_exposition_forward_asynch_interrupted_tot_lp += value;
			break;
		case STAT_CLOCK_EXPOSITION_SILENT_ASYNCH_INTERRUPTED_LP:
			stats[idx].clock_exposition_silent_asynch_interrupted_tot_lp += value;
			break;
		case STAT_CLOCK_EXPOSITION_FORWARD_SYNCH_INTERRUPTED_LP:
			stats[idx].clock_exposition_forward_synch_interrupted_tot_lp += value;
			break;
		case STAT_CLOCK_EXPOSITION_SILENT_SYNCH_INTERRUPTED_LP:
			stats[idx].clock_exposition_silent_synch_interrupted_tot_lp += value;
			break;


		case STAT_CLOCK_RESIDUAL_TIME_FORWARD_ASYNCH_GAINED_LP:
			stats[idx].clock_residual_time_forward_asynch_gained_tot_lp += value;
			break;
		case STAT_CLOCK_RESIDUAL_TIME_SILENT_ASYNCH_GAINED_LP:
			stats[idx].clock_residual_time_silent_asynch_gained_tot_lp += value;
			break;
		case STAT_CLOCK_RESIDUAL_TIME_FORWARD_SYNCH_GAINED_LP:
			stats[idx].clock_residual_time_forward_synch_gained_tot_lp += value;
			break;
		case STAT_CLOCK_RESIDUAL_TIME_SILENT_SYNCH_GAINED_LP:
			stats[idx].clock_residual_time_silent_synch_gained_tot_lp += value;
			break;

		case STAT_LATENCY_START_EXPOSITION_AND_SEND_IPI_TID:
			stats[idx].latency_start_exposition_and_send_ipi_tot_tid += value;
			break;
		case STAT_NUM_CLOCK_DRIFT_TID:
			stats[idx].num_clock_drift_tot_tid += value;
            break;
        case STAT_CLOCK_DRIFT_TID:
            stats[idx].clock_drift_tot_tid += value;
            break;
        case STAT_MAX_CLOCK_DRIFT_TID:
			if(stats[idx].max_clock_drift_tid < value)
				stats[idx].max_clock_drift_tid = value;
            break;
		#endif

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
		system_stats->infos_posted_tid += 		thread_stats[i].infos_posted_tid;//per thread num info posted by thread
		system_stats->infos_posted_attempt_tid += 		thread_stats[i].infos_posted_attempt_tid;//per thread num info posted by thread
		system_stats->infos_posted_useful_tid += thread_stats[i].infos_posted_useful_tid;//per thread num info useful for thread
		#endif

		#if IPI_SUPPORT && REPORT==1
		system_stats->ipi_sent_tid += thread_stats[i].ipi_sent_tid;
		system_stats->ipi_trampoline_received_tid += thread_stats[i].ipi_trampoline_received_tid;
		system_stats->ipi_received_tid += thread_stats[i].ipi_received_tid;
		system_stats->clock_exec_ipi_syscall_tid += thread_stats[i].clock_exec_ipi_syscall_tid;
		#endif

		#if IPI_DECISION_MODEL==1 && REPORT==1
		system_stats->ipi_filtered_in_decision_model_tid += thread_stats[i].ipi_filtered_in_decision_model_tid;
		system_stats->latency_start_exposition_and_send_ipi_tot_tid += thread_stats[i].latency_start_exposition_and_send_ipi_tot_tid;
		
		system_stats->num_clock_drift_tot_tid += thread_stats[i].num_clock_drift_tot_tid;
    	system_stats->clock_drift_tot_tid += thread_stats[i].clock_drift_tot_tid;
    	if(i==0)
    		system_stats->max_clock_drift_global = thread_stats[i].max_clock_drift_tid;
    	else if(system_stats->max_clock_drift_global < thread_stats[i].max_clock_drift_tid)
    		system_stats->max_clock_drift_global = thread_stats[i].max_clock_drift_tid;
		#endif
	}

	system_stats->clock_prune /= system_stats->counter_prune;
	system_stats->clock_loop_tot = system_stats->clock_loop;
	system_stats->clock_loop /= n_cores;
	
	#if POSTING==1 && REPORT==1
	system_stats->infos_posted_tot = system_stats->infos_posted_tid;
    system_stats->infos_posted_tid/= n_cores;

    system_stats->infos_posted_attempt_tot = system_stats->infos_posted_attempt_tid;//per lp num info posted by lp
    system_stats->infos_posted_attempt_tid/= n_cores;

    system_stats->infos_posted_useful_tot = system_stats->infos_posted_useful_tid;//per thread num info useful for thread
    system_stats->infos_posted_useful_tid/= n_cores;
	#endif

	#if IPI_SUPPORT==1 && REPORT==1
	system_stats->ipi_sent_tot = system_stats->ipi_sent_tid;
	system_stats->ipi_trampoline_received_tot = system_stats->ipi_trampoline_received_tid;
	system_stats->ipi_received_tot = system_stats->ipi_received_tid;

	system_stats->ipi_sent_tid /= n_cores;
	system_stats->ipi_trampoline_received_tid /= n_cores;
	system_stats->ipi_received_tid /= n_cores;

	system_stats->clock_exec_ipi_syscall_tot=system_stats->clock_exec_ipi_syscall_tid;
	system_stats->clock_exec_ipi_syscall_tid/=n_cores;
	system_stats->clock_exec_ipi_syscall_per_syscall=system_stats->clock_exec_ipi_syscall_tot/system_stats->ipi_sent_tot;
	#endif

	#if IPI_DECISION_MODEL==1 && REPORT==1
	system_stats->ipi_filtered_in_decision_model_tot = system_stats->ipi_filtered_in_decision_model_tid;
	system_stats->ipi_filtered_in_decision_model_tid /= n_cores;
	system_stats->latency_start_exposition_and_send_ipi_per_ipi_sent = system_stats->latency_start_exposition_and_send_ipi_tot_tid/system_stats->ipi_sent_tot;
	system_stats->avg_clock_drift_per_clock_drift = system_stats->clock_drift_tot_tid/system_stats->num_clock_drift_tot_tid;
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
		system_stats->events_silent_for_gvt += lp_stats[i].events_silent_for_gvt;
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

	#if STATISTICS_ADDED==1 && REPORT==1
    	system_stats->events_exec_and_committed_lp += lp_stats[i].events_exec_and_committed_lp;
    #endif

	#if HANDLE_INTERRUPT==1 && REPORT==1
		system_stats->event_not_flushed_lp  += lp_stats[i].event_not_flushed_lp;//per lp event that lp father doesn't flush
		
		system_stats->clock_exposition_silent_tot_lp += lp_stats[i].clock_exposition_silent_tot_lp;
		system_stats->clock_exposition_forward_tot_lp += lp_stats[i].clock_exposition_forward_tot_lp;
		
		system_stats->event_exposition_silent_lp += lp_stats[i].event_exposition_silent_lp;
		system_stats->event_exposition_forward_lp += lp_stats[i].event_exposition_forward_lp;
		
		system_stats->event_resume_rollback_tot_lp += lp_stats[i].event_resume_rollback_tot_lp;
		system_stats->count_resume_rollback_tot_lp += lp_stats[i].count_resume_rollback_tot_lp;

		system_stats->clock_resume_rollback_tot_lp += lp_stats[i].clock_resume_rollback_tot_lp;

	#endif
	#if IPI_DECISION_MODEL==1 && REPORT==1
		system_stats->event_exposition_forward_asynch_interrupted_lp += lp_stats[i].event_exposition_forward_asynch_interrupted_lp;
    	system_stats->event_exposition_silent_asynch_interrupted_lp += lp_stats[i].event_exposition_silent_asynch_interrupted_lp;

    	system_stats->event_exposition_forward_synch_interrupted_lp += lp_stats[i].event_exposition_forward_synch_interrupted_lp;
    	system_stats->event_exposition_silent_synch_interrupted_lp += lp_stats[i].event_exposition_silent_synch_interrupted_lp;
	


		system_stats->clock_exposition_forward_asynch_interrupted_tot_lp += lp_stats[i].clock_exposition_forward_asynch_interrupted_tot_lp;
		system_stats->clock_exposition_silent_asynch_interrupted_tot_lp += lp_stats[i].clock_exposition_silent_asynch_interrupted_tot_lp;

		system_stats->clock_exposition_forward_synch_interrupted_tot_lp += lp_stats[i].clock_exposition_forward_synch_interrupted_tot_lp;
		system_stats->clock_exposition_silent_synch_interrupted_tot_lp += lp_stats[i].clock_exposition_silent_synch_interrupted_tot_lp;
	
		system_stats->clock_residual_time_forward_asynch_gained_tot_lp += lp_stats[i].clock_residual_time_forward_asynch_gained_tot_lp;
		system_stats->clock_residual_time_silent_asynch_gained_tot_lp += lp_stats[i].clock_residual_time_silent_asynch_gained_tot_lp;
		system_stats->clock_residual_time_forward_synch_gained_tot_lp += lp_stats[i].clock_residual_time_forward_synch_gained_tot_lp;
		system_stats->clock_residual_time_silent_synch_gained_tot_lp += lp_stats[i].clock_residual_time_silent_synch_gained_tot_lp;
	
	#endif

	#if HANDLE_INTERRUPT_WITH_CHECK==1 && REPORT==1
    	system_stats->sync_check_silent_lp += lp_stats[i].sync_check_silent_lp;//per lp num sync_check in past maded by lp
    	system_stats->sync_check_forward_lp += lp_stats[i].sync_check_forward_lp;//per lp num sync_check in future maded by lp
    	system_stats->sync_check_useful_lp += lp_stats[i].sync_check_useful_lp;//per lp num sync_check useful maded by lp
    	
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
	// TODO rivedere questa statistica, perché incrementa su lp_stats[i] è già stata calcolata dentro il for degli lp
	//system_stats->counter_rollbacks_length    += lp_stats[i].counter_rollbacks_length;
	system_stats->clock_checkpoint   /= system_stats->counter_checkpoints;
	system_stats->clock_recovery     /= system_stats->counter_recoveries;

	system_stats->clock_safe         /= (system_stats->events_total - system_stats->events_silent);
	system_stats->clock_frame_tot     = system_stats->clock_safe * system_stats->total_frames;

	system_stats->events_get_next_fetch /= system_stats->events_fetched;	
	system_stats->clock_enqueue       /= system_stats->events_enqueued;
		

	system_stats->counter_checkpoint_recalc /= n_prc_tot;

	system_stats->mem_checkpoint /= system_stats->counter_checkpoints;
	system_stats->checkpoint_period /= n_prc_tot;

	//new statistics added
    #if STATISTICS_ADDED==1 && REPORT==1
    system_stats->events_exec_and_committed_tot= system_stats->events_exec_and_committed_lp;
    system_stats->events_exec_and_committed_lp /= n_prc_tot;

    #endif

	#if HANDLE_INTERRUPT==1 && REPORT==1
	system_stats->event_not_flushed_tot = system_stats->event_not_flushed_lp;//per lp event that lp father doesn't flush
    system_stats->event_not_flushed_lp/= n_prc_tot;
    
    system_stats->event_exposition_forward_tot=system_stats->event_exposition_forward_lp;
    system_stats->event_exposition_forward_lp /= n_prc_tot;

    system_stats->event_exposition_silent_tot=system_stats->event_exposition_silent_lp;
    system_stats->event_exposition_silent_lp /= n_prc_tot;

    system_stats->clock_exposition_forward_per_event = system_stats->clock_exposition_forward_tot_lp/ system_stats->event_exposition_forward_tot;

    system_stats->clock_exposition_silent_per_event=system_stats->clock_exposition_silent_tot_lp/system_stats->event_exposition_silent_tot;

    system_stats->avg_event_resume_per_rollback_resume = system_stats->event_resume_rollback_tot_lp/system_stats->count_resume_rollback_tot_lp;
    system_stats->avg_clock_resume_per_rollback_resume = system_stats->clock_resume_rollback_tot_lp/system_stats->count_resume_rollback_tot_lp;

    #endif

    #if IPI_DECISION_MODEL==1 && REPORT==1
    system_stats->event_exposition_forward_asynch_interrupted_tot= system_stats->event_exposition_forward_asynch_interrupted_lp;
    system_stats->event_exposition_forward_asynch_interrupted_lp /= n_prc_tot;

    system_stats->event_exposition_forward_synch_interrupted_tot= system_stats->event_exposition_forward_synch_interrupted_lp;
    system_stats->event_exposition_forward_synch_interrupted_lp /= n_prc_tot;

    system_stats->event_exposition_silent_asynch_interrupted_tot= system_stats->event_exposition_silent_asynch_interrupted_lp;
    system_stats->event_exposition_silent_asynch_interrupted_lp /= n_prc_tot;

    system_stats->event_exposition_silent_synch_interrupted_tot= system_stats->event_exposition_silent_synch_interrupted_lp;
    system_stats->event_exposition_silent_synch_interrupted_lp /= n_prc_tot;



    system_stats->clock_exposition_forward_asynch_interrupted_per_event = system_stats->clock_exposition_forward_asynch_interrupted_tot_lp/system_stats->event_exposition_forward_asynch_interrupted_tot;
    system_stats->clock_exposition_silent_asynch_interrupted_per_event = system_stats->clock_exposition_silent_asynch_interrupted_tot_lp/system_stats->event_exposition_silent_asynch_interrupted_tot;

    system_stats->clock_exposition_forward_synch_interrupted_per_event = system_stats->clock_exposition_forward_synch_interrupted_tot_lp/system_stats->event_exposition_forward_synch_interrupted_tot;
    system_stats->clock_exposition_silent_synch_interrupted_per_event = system_stats->clock_exposition_silent_synch_interrupted_tot_lp/system_stats->event_exposition_silent_synch_interrupted_tot;



    system_stats->clock_residual_time_forward_asynch_gained_per_event = system_stats->clock_residual_time_forward_asynch_gained_tot_lp/system_stats->event_exposition_forward_asynch_interrupted_tot;
    system_stats->clock_residual_time_silent_asynch_gained_per_event = system_stats->clock_residual_time_silent_asynch_gained_tot_lp/system_stats->event_exposition_silent_asynch_interrupted_tot;

    system_stats->clock_residual_time_forward_synch_gained_per_event = system_stats->clock_residual_time_forward_synch_gained_tot_lp/system_stats->event_exposition_forward_synch_interrupted_tot;
    system_stats->clock_residual_time_silent_synch_gained_per_event = system_stats->clock_residual_time_silent_synch_gained_tot_lp/system_stats->event_exposition_silent_synch_interrupted_tot;    
    

    system_stats->equivalent_events_forward_interrupted_tot = (system_stats->clock_residual_time_forward_asynch_gained_tot_lp+system_stats->clock_residual_time_forward_synch_gained_tot_lp)/system_stats->clock_exposition_forward_per_event;
    system_stats->equivalent_events_silent_interrupted_tot = (system_stats->clock_residual_time_silent_asynch_gained_tot_lp+system_stats->clock_residual_time_silent_synch_gained_tot_lp)/ system_stats->clock_exposition_silent_per_event;
    #endif

	#if HANDLE_INTERRUPT_WITH_CHECK==1 && REPORT==1

    system_stats->sync_check_silent_tot  =  system_stats->sync_check_silent_lp;//per lp num sync_check in past maded by lp
    system_stats->sync_check_silent_lp/= n_prc_tot;

    system_stats->sync_check_forward_tot  =  system_stats->sync_check_forward_lp;//per lp num sync_check in future maded by lp
    system_stats->sync_check_forward_lp /= n_prc_tot;

    system_stats->sync_check_useful_tot = system_stats->sync_check_useful_lp;
    system_stats->sync_check_useful_lp /= n_prc_tot;
    #endif

}

static void _print_statistics(struct stats_t *stats) {

	printf(COLOR_CYAN);

	printf("Total events....................................: %12llu\n", (unsigned long long)stats->events_total);
	printf("Committed events................................: %12llu (%4.2f%%)\n",
		(unsigned long long)stats->events_committed, percentage(stats->events_committed, stats->events_total));
	
	#if STATISTICS_ADDED==1 && REPORT==1
	printf("Events executed and committed tot...............: %12lu\n",
		(unsigned long)stats->events_exec_and_committed_tot);
	#endif

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
	printf("AVG Rollbacked Events per Rollback..............: %12.2f\n", stats->counter_rollbacks_length/stats->counter_rollbacks);
	printf("AVG Reprocessed Events per Rollback.............: %12.2f\n", ((double)stats->events_silent)/stats->counter_rollbacks);
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
	
	printf("\n");

	#if POSTING==1 && REPORT==1

    printf("Num attempts Info posted tot....................: %12lu\n",
		(unsigned long)stats->infos_posted_attempt_tot);

	printf("Info posted tot.................................: %12lu\n",
		(unsigned long)stats->infos_posted_tot);

	printf("Info posted useful tot..........................: %12lu\n",
		(unsigned long)stats->infos_posted_useful_tot);

	printf("\n");
	#endif

	#if HANDLE_INTERRUPT==1 && REPORT==1
	
	printf("Event not flushed tot...........................: %12lu\n",
		(unsigned long)stats->event_not_flushed_tot);

	printf("Exposition forward Time tot.....................: %12lu clocks\n",
		(unsigned long)stats->clock_exposition_forward_tot_lp);
	printf("Exposition forward Time per event...............: %12.2f clocks\n",
		stats->clock_exposition_forward_per_event);

	printf("Exposition silent Time tot......................: %12lu clocks\n",
		(unsigned long)stats->clock_exposition_silent_tot_lp);
	printf("Exposition silent Time per event................: %12.2f clocks\n",
		stats->clock_exposition_silent_per_event);

	printf("\n");
	
	printf("Total resumable rollback executed...............: %12lu\n",
		(unsigned long)stats->count_resume_rollback_tot_lp);
	printf("Avg events resume per rollback_resume...........: %12.2f\n",
		stats->avg_event_resume_per_rollback_resume);
    printf("Avg clock resume per rollback resume............: %12.2f clocks\n",
		stats->avg_clock_resume_per_rollback_resume);
	printf("\n");

	#endif

	#if IPI_DECISION_MODEL==1 && REPORT==1
	printf("Events forward asynch interrupted tot... .......: %12lu\n",
		(unsigned long)stats->event_exposition_forward_asynch_interrupted_tot);

	printf("Events forward synch interrupted tot............: %12lu\n",
		(unsigned long)stats->event_exposition_forward_synch_interrupted_tot);

	printf("Events silent asynch interrupted tot............: %12lu\n",
		(unsigned long)stats->event_exposition_silent_asynch_interrupted_tot);

	printf("Events silent synch interrupted tot.............: %12lu\n",
		(unsigned long)stats->event_exposition_silent_synch_interrupted_tot);

	printf("\n");

	printf("Exposition forward asynch interrupted Time per event...: %12lf clocks\n",
		stats->clock_exposition_forward_asynch_interrupted_per_event);
	printf("Exposition silent asynch interrupted Time per event....: %12.2f clocks\n",
		stats->clock_exposition_silent_asynch_interrupted_per_event);

	printf("Exposition forward synch interrupted Time per event....: %12lf clocks\n",
		stats->clock_exposition_forward_synch_interrupted_per_event);
	printf("Exposition silent synch interrupted Time per event.....: %12.2f clocks\n",
		stats->clock_exposition_silent_synch_interrupted_per_event);

	printf("Residual Time forward asynch gained per event..........: %12lf clocks\n",
		stats->clock_residual_time_forward_asynch_gained_per_event);
	printf("Residual Time forward synch gained per event...........: %12lf clocks\n",
		stats->clock_residual_time_forward_synch_gained_per_event);

	printf("Residual Time silent asynch gained per event...........: %12.2f clocks\n",
		stats->clock_residual_time_silent_asynch_gained_per_event);
	printf("Residual Time silent synch gained per event............: %12.2f clocks\n",
		stats->clock_residual_time_silent_synch_gained_per_event);

	printf("\n");
    printf("Equivalent events forward entirely interrupted.........: %12lf\n",
		stats->equivalent_events_forward_interrupted_tot);
	printf("Equivalent events silent entirely interrupted..........: %12.2f\n",
		stats->equivalent_events_silent_interrupted_tot);
	printf("Avg Latency to send ipi................................: %12.2f clocks\n",
		stats->latency_start_exposition_and_send_ipi_per_ipi_sent);

    printf("Num clock drift tot....................................: %12lu\n",
		(unsigned long)stats->num_clock_drift_tot_tid);
    printf("Max clock drift global.................................: %12lu\n",
		(unsigned long)stats->max_clock_drift_global);
    printf("Avg Clock drift per clock_drift........................: %12.2f clocks\n",
		stats->avg_clock_drift_per_clock_drift);
	printf("\n");

    #endif

	#if SYNCH_CHECK==1 && REPORT==1
	printf("Sync check Forward Execution tot................: %12lu\n",
		(unsigned long)stats->sync_check_forward_tot);
	printf("Sync check Silent Execution tot.................: %12lu\n",
		(unsigned long)stats->sync_check_silent_tot);
	printf("Sync check useful tot...........................: %12lu\n",
		(unsigned long)stats->sync_check_useful_tot);

	printf("\n");
	#endif

	#if IPI_SUPPORT==1 && REPORT==1
	printf("IPI sent tot....................................: %12lu\n",
		(unsigned long)stats->ipi_sent_tot);
	printf("IPI handled in trampoline tot...................: %12lu\n",
		(unsigned long)stats->ipi_trampoline_received_tot);
	printf("IPI received tot................................: %12lu\n",
		(unsigned long)stats->ipi_received_tot);

	printf("\n");

	printf("IPI sent syscall time...........................: %12lu clocks\n",
		(unsigned long)stats->clock_exec_ipi_syscall_tot);
	printf("IPI sent syscall time per 1 syscall.............: %12.2f clocks\n",
		stats->clock_exec_ipi_syscall_per_syscall);
	printf("\n");
	#endif

	#if IPI_DECISION_MODEL==1 && REPORT==1
	printf("IPI filtered in decision_model tot..............: %12lu\n",
		(unsigned long)stats->ipi_filtered_in_decision_model_tot);
	printf("\n\n");
	#endif

    printf("Total allocated space...........................: %lu MB\n", get_memory_allocated()/(1024));

	printf("\n\n");

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
	#if PRINT_IPI_DECISION_MODEL_STATS==1
	print_ipi_decision_model_stats();
	#endif
	
	printf("===== SYSTEM-WIDE STATISTICS =====\n");
	_print_statistics(system_stats);
	
}

#if REPORT==1
void write_empty_cell_and_separator(FILE*results_file,char*separator){
	fprintf(results_file,"Empty%s",separator);
}

void write_unsigned_and_separator(FILE*file,unsigned int value,char*separator){
	fprintf(file,"%u%s",value,separator);
}

void write_double_and_separator(FILE*file,double value,char*separator){
	fprintf(file,"%.2f%s",value,separator);
}

void write_string_unsigned_and_separator(FILE*file,unsigned int value,char*string,char*separator){
	fprintf(file,"%s %u%s",string,value,separator);
}

void write_string_double_and_separator(FILE*file,double value,char*string,char*separator){
	fprintf(file,"%s %.2f%s",string,value,separator);
}

void write_standard_statistics(FILE*results_file){
	write_unsigned_and_separator(results_file,n_cores,",");//num_thread
	write_unsigned_and_separator(results_file,n_prc_tot,",");//num_lps
	write_double_and_separator(results_file,simduration,",");//sim_duration
    write_unsigned_and_separator(results_file,CHECKPOINT_PERIOD,",");//checkpoint_period
    write_double_and_separator(results_file,(double)(system_stats->events_committed)/simduration/n_cores,",");//committed/sec
    write_double_and_separator(results_file,percentage(system_stats->events_straggler, system_stats->events_total),",");//%straggler
    write_double_and_separator(results_file,percentage(system_stats->events_anti, system_stats->events_total),",");//% anti
    write_double_and_separator(results_file,percentage((system_stats->events_total - system_stats->events_silent - system_stats->total_frames+n_prc_tot), system_stats->events_total),",");//%useless
    write_double_and_separator(results_file,percentage(system_stats->events_silent, system_stats->events_total),",");//% silent
    write_double_and_separator(results_file,system_stats->clock_event,",");//event grain
    write_double_and_separator(results_file,system_stats->clock_safe,",");//event forward grain
    write_double_and_separator(results_file,system_stats->clock_silent,",");//event silent grain
}

#if IPI_SUPPORT==1
void write_ipi_statistics(FILE*results_file){
	write_string_unsigned_and_separator(results_file,system_stats->ipi_sent_tot,"Num_sent",";");//num_ipi
	write_string_unsigned_and_separator(results_file,system_stats->ipi_trampoline_received_tot,"Num_rcv in tramp",";");//num ipi trampoline
	write_string_unsigned_and_separator(results_file,system_stats->ipi_received_tot,"Num_rcv",";");//num ipi_received tot
	write_string_double_and_separator(results_file,system_stats->clock_exec_ipi_syscall_per_syscall,"Time 1 sys",",");
}
#endif

#if POSTING==1
void write_posting_statistics(FILE*results_file){
	write_string_unsigned_and_separator(results_file,system_stats->infos_posted_attempt_tot,"Attempt",";");
	write_string_unsigned_and_separator(results_file,system_stats->infos_posted_tot,"Success",",");
}
#endif

void write_first_line_on_csv(FILE*results_file){
	fprintf(results_file,"MODEL,THREADS,LPS,SEC,CKPT,COMMIT/sec,%%STRAG,%%ANTI,%%USELESS,%%SILENT,TIME EVT,TIME FORWARD,TIME SILENT,POSTING,IPI,END\n");
}

//remember to adjust first line of csv table
void write_results_on_csv(char*path){
	return;
	FILE* results_file;
	if( access( path, F_OK ) != -1 ) 
	{//file exist
    	results_file=fopen(path,"a+");
    	if(results_file==NULL){
			printf("error in fopen file %s\n",path);
			gdb_abort;
		}
	} 
	else 
	{//file not exist
    	results_file=fopen(path,"a+");
    	if(results_file==NULL){
			printf("error in fopen file %s\n",path);
			gdb_abort;
		}
		write_first_line_on_csv(results_file);
	}
	write_model_parameters_and_separator(results_file,",");
	write_standard_statistics(results_file);//common at all simulation model
	

	#if POSTING==1 
	write_posting_statistics(results_file);
	#else
	write_empty_cell_and_separator(results_file,",");
	#endif
	#if IPI_SUPPORT==1
	write_ipi_statistics(results_file);
	#else
	write_empty_cell_and_separator(results_file,",");
	#endif

	write_empty_cell_and_separator(results_file,"\n");
    fclose(results_file);
    
    return;

}
#endif//REPORT