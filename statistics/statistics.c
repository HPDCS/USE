#include <stdio.h>

#include <statistics.h>
#include <core.h>


//Variabili da tunare durante l'esecuzione per throttling e threshold
extern double delta_count;
extern unsigned int reverse_execution_threshold_main;

struct stats_t *thread_stats;
struct stats_t system_stats;

//unsigned long long events_fetched;
//unsigned long long events_flushed;
simtime_t tot_time_between_events;
simtime_t t_btw_evts;

void statistics_init() {
    //thread_stats = malloc(n_cores * sizeof(struct stats_t));
    if(posix_memalign((void **)&thread_stats, 64, n_cores * sizeof(struct stats_t)) < 0) {
	    printf("memalign failed\n");
    }

    if(thread_stats == NULL) {
        printf("Unable to allocate statistics vector\n");
        abort();
    }
    memset(thread_stats, 0, n_cores * sizeof(struct stats_t));
    
    memset(&system_stats, 0, sizeof(struct stats_t));
    //events_fetched = 0;
    tot_time_between_events = 0;
}

void statistics_fini() {
    free(thread_stats);
}


void statistics_post_data(int tid, int type, double value) {

    switch(type) {
        case EVENTS_SAFE:
            thread_stats[tid].events_total++;
            thread_stats[tid].events_safe++;
            break;

        case EVENTS_STM:
            thread_stats[tid].events_total++;
            thread_stats[tid].events_stm++;
            break;
            
        case EVENTS_ROLL:
            thread_stats[tid].events_roll++;
            break;

        case COMMITS_STM:
            thread_stats[tid].commits_stm++;
            thread_stats[tid].commits_stm_tmp++;
            break;

        case CLOCK_DEQUEUE:
            thread_stats[tid].clock_dequeue += (unsigned long long)value;
            break;

        case CLOCK_DEQ_LP:
            thread_stats[tid].clock_deq_lp += (unsigned long long)value;
            break;

        case CLOCK_ENQUEUE:
            thread_stats[tid].clock_enqueue += (unsigned long long)value;
            break;

        case CLOCK_DELETE:
            thread_stats[tid].clock_delete += (unsigned long long)value;
            break;

        case CLOCK_LOOP:
            thread_stats[tid].clock_loop += (unsigned long long)value;
            break;

        case CLOCK_SAFE:
            thread_stats[tid].clock_safe += (unsigned long long)value;
            break;
            
        case CLOCK_STM:
            thread_stats[tid].clock_stm += (unsigned long long)value;
            thread_stats[tid].clock_stm_tmp += (unsigned long long)value;
            break;

        case CLOCK_STM_WAIT:
            thread_stats[tid].clock_stm_wait += (unsigned long long)value;
            break;

        case CLOCK_UNDO_EVENT:
            thread_stats[tid].clock_undo_event += (unsigned long long)value;
            break;

        case EVENTS_FETCHED:
			thread_stats[tid].events_fetched++;
			break;

        case EVENTS_FLUSHED:
			thread_stats[tid].events_flushed++;
			break;
			
		case T_BTW_EVT:
//			tot_time_between_events += value;
//			t_btw_evts = tot_time_between_events/events_fetched;
			break;

        default:
            printf("Unrecognized stat type (%d)\n", type);
    }
}

unsigned long long get_time_of_an_event(){
	return (thread_stats[tid].clock_htm-thread_stats[tid].clock_htm_throttle)/(thread_stats[tid].events_htm+1);
	return thread_stats[tid].clock_safe/(thread_stats[tid].events_safe+1);
}

double get_frac_htm_aborted(){
	return thread_stats[tid].abort_conflict+thread_stats[tid].abort_unsafe/thread_stats[tid].events_htm;
}

unsigned long long get_useful_time_htm(){
	return get_useful_time_htm_th(tid);
}
unsigned long long get_useful_time_htm_th(unsigned int t){
	unsigned long long tmp = (unsigned long long) thread_stats[t].clock_htm_tmp/(thread_stats[t].commits_htm_tmp+1);
	thread_stats[t].clock_htm_tmp = 0;
	thread_stats[t].commits_htm_tmp = 0;
	return tmp;
}
unsigned long long get_useful_time_stm(){
	return get_useful_time_stm_th(tid);
}
unsigned long long get_useful_time_stm_th(unsigned int t){
	unsigned long long tmp = (unsigned long long) thread_stats[t].clock_stm_tmp/(thread_stats[t].commits_stm_tmp+1);
	thread_stats[t].clock_stm_tmp = 0;
	thread_stats[t].commits_stm_tmp = 0;
	return tmp;
}

void gather_statistics() {
    unsigned int i;

    for(i = 0; i < n_cores; i++) {
        system_stats.events_total += thread_stats[i].events_total;
        system_stats.events_safe += thread_stats[i].events_safe;
        system_stats.events_stm += thread_stats[i].events_stm;
        system_stats.events_roll += thread_stats[i].events_roll;

        system_stats.commits_stm += thread_stats[i].commits_stm;

        system_stats.clock_safe += thread_stats[i].clock_safe;
        system_stats.clock_htm += thread_stats[i].clock_htm;
        system_stats.clock_stm += thread_stats[i].clock_stm;
        system_stats.clock_enqueue += thread_stats[i].clock_enqueue;
        system_stats.clock_dequeue += thread_stats[i].clock_dequeue;
        system_stats.clock_deq_lp += thread_stats[i].clock_deq_lp;
        system_stats.clock_delete += thread_stats[i].clock_delete;
        system_stats.clock_loop += thread_stats[i].clock_loop;
        system_stats.clock_stm_wait += thread_stats[i].clock_stm_wait;
        system_stats.clock_undo_event += thread_stats[i].clock_undo_event;

        system_stats.abort_retry += thread_stats[i].abort_retry;
        
        system_stats.events_fetched += thread_stats[i].events_fetched;
        system_stats.events_flushed += thread_stats[i].events_flushed;
    }
		if(system_stats.events_safe != 0) {
		        system_stats.clock_safe /= system_stats.events_safe;
		}

		if(system_stats.commits_stm != 0) {
		        system_stats.clock_stm /= system_stats.commits_stm;
		        system_stats.clock_stm_wait /= system_stats.commits_stm;
		}
		
		if(system_stats.events_roll != 0) {
		        system_stats.clock_undo_event /= system_stats.events_roll;
		}

		if (system_stats.abort_reverse != 0) {
			system_stats.clock_undo_event /= system_stats.abort_reverse;
		}
}

void print_statistics() {

#if REPORT == 1
    gather_statistics();

    printf("Simulation final report:\n");
    printf("Thread: %d\tLP: %d\t\n", n_cores, n_prc_tot);
    //printf("Lookahead: %d", 0);
    printf("\n");

    printf("Total events....................................: %12u\n", system_stats.events_total);
    printf("Safe events.....................................: %12u (%.2f%%)\n", system_stats.events_safe, ((double)system_stats.events_safe / system_stats.events_total)*100);
    printf("STM events......................................: %12u (%.2f%%)\n\n", system_stats.events_stm, ((double)system_stats.events_stm / system_stats.events_total)*100);
    
    unsigned int commits_total = system_stats.events_safe + system_stats.commits_htm + system_stats.commits_stm;
    
    printf("TOT committed...................................: %12u\n", commits_total);
    printf("STM committed...................................: %12u (%.2f%%)\n\n", system_stats.commits_stm, ((double)system_stats.commits_stm / system_stats.events_stm)*100);
	printf("STM rollbacked..................................: %12u (%.2f%%)\n\n", system_stats.events_roll, ((double)system_stats.events_roll / system_stats.events_stm)*100);

    printf("Average time spent in safe execution............: %12llu clocks\n", system_stats.clock_safe);

//NOTA: queste info derivano dai soli eventi arrivati a commit, non da quelli rollbackati
    printf("Average time spent committed STM execution......: %12llu clocks\n", system_stats.clock_stm);
    printf("    Avg time spent to EXE  STM event............: %12llu clocks (%.2f%%)\n", (system_stats.clock_stm-system_stats.clock_stm_wait), ((double)(system_stats.clock_stm-system_stats.clock_stm_wait) / system_stats.clock_stm)*100);
    printf("    Avg time spent to WAIT STM safety...........: %12llu clocks (%.2f%%)\n", system_stats.clock_stm_wait, ((double)system_stats.clock_stm_wait / system_stats.clock_stm)*100);
    printf("    Avg time spent to ROLLBACK STM event........: %12llu clocks\n\n", system_stats.clock_undo_event);

//    printf("Average useful time spent in STM execution....: %.2f clocks\n\n", system_stats.clock_stm/((double)system_stats.commits_stm / system_stats.events_stm));
    
	printf("Time spent in enqueue...........................: %12llu clocks \tavg %12llu clocks\n", system_stats.clock_enqueue, ((unsigned long long)system_stats.clock_enqueue/system_stats.events_flushed));
	printf("Time spent in dequeue...........................: %12llu clocks \tavg %12llu clocks\n", system_stats.clock_dequeue, ((unsigned long long)system_stats.clock_dequeue/system_stats.events_fetched));
	printf("Time spent in deletion..........................: %12llu clocks \tavg %12llu clocks\n", system_stats.clock_delete, ((unsigned long long)system_stats.clock_delete/system_stats.events_total));
	printf("Time spent in dequeue_lp........................: %12llu clocks\n\n", system_stats.clock_deq_lp);
	
	printf("Time spent in main loop.........................: %12llu clocks\n", system_stats.clock_loop);

    unsigned int abort_total = system_stats.abort_unsafe +
        system_stats.abort_conflict +
        system_stats.abort_cachefull +
        system_stats.abort_nested +
        system_stats.abort_debug;

    system_stats.abort_generic = system_stats.abort_total - abort_total;

 //   printf("Total HTM aborts................................: %11u (%.2f%%)\n", system_stats.abort_total,  ((double)system_stats.abort_total/(double)system_stats.events_htm)*100);
 //
 //   printf("HTM aborts for UNSAFETY.........................: %11u (%.2f%%)\n", system_stats.abort_unsafe, ((double)system_stats.abort_unsafe / abort_total)*100);
 //   printf("HTM aborts for CONFLICT.........................: %11u (%.2f%%)\n", system_stats.abort_conflict, ((double)system_stats.abort_conflict / abort_total)*100);
 //   printf("HTM aborts for CACHEFULL........................: %11u (%.2f%%)\n", system_stats.abort_cachefull, ((double)system_stats.abort_cachefull / abort_total)*100);
 //   printf("HTM aborts for NESTED...........................: %11u (%.2f%%)\n", system_stats.abort_nested, ((double)system_stats.abort_nested / abort_total)*100);
 //   printf("HTM aborts for DEBUG............................: %11u (%.2f%%)\n", system_stats.abort_debug, ((double)system_stats.abort_debug / abort_total)*100);
 //   printf("HTM aborts for GENERIC..........................: %11u (%.2f%%)\n", system_stats.abort_generic, ((double)system_stats.abort_generic / abort_total)*100);
 //   printf("HTM abort RETRY.................................: %11u (%.2f%%)\n\n", system_stats.abort_retry, ((double)system_stats.abort_retry / abort_total)*100);
 //
 //   printf("STM abort for UNSAFETY..........................: %11u\n\n", system_stats.abort_reverse);
#endif
}

