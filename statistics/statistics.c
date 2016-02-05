#include <stdio.h>

#include <statistics.h>
#include <core.h>


//Variabili da tunare durante l'esecuzione per throttling e threshold
extern double delta_count;
extern unsigned int reverse_execution_threshold;

struct stats_t *thread_stats;
struct stats_t system_stats;

unsigned long long events_fetched;
simtime_t tot_time_between_events;
simtime_t t_btw_evts;

void statistics_init() {
    //thread_stats = malloc(n_cores * sizeof(struct stats_t));
    if(posix_memalign(&thread_stats, 64, n_cores * sizeof(struct stats_t)) < 0) {
	    printf("memalign failed\n");
    }

    if(thread_stats == NULL) {
        printf("Unable to allocate statistics vector\n");
        abort();
    }
    memset(thread_stats, 0, n_cores * sizeof(struct stats_t));

    memset(&system_stats, 0, sizeof(struct stats_t));
    
    events_fetched = 0;
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

        case EVENTS_HTM:
            thread_stats[tid].events_total++;
            thread_stats[tid].events_htm++;
            break;

        case EVENTS_STM:
            thread_stats[tid].events_total++;
            thread_stats[tid].events_stm++;
            break;

        case COMMITS_HTM:
            thread_stats[tid].commits_htm++;
            break;

        case COMMITS_STM:
            thread_stats[tid].commits_stm++;
            break;

        case CLOCK_SAFE:
            thread_stats[tid].clock_safe += (unsigned long long)value;
            break;

        case CLOCK_HTM:
            thread_stats[tid].clock_htm += (unsigned long long)value;
            break;

        case CLOCK_STM:
            thread_stats[tid].clock_stm += (unsigned long long)value;
            break;

        case CLOCK_HTM_THROTTLE:
            thread_stats[tid].clock_htm_throttle += (unsigned long long)value;
            break;

        case CLOCK_STM_WAIT:
            thread_stats[tid].clock_stm_wait += (unsigned long long)value;
            break;

        case CLOCK_UNDO_EVENT:
            thread_stats[tid].clock_undo_event += (unsigned long long)value;
            break;

        case ABORT_UNSAFE:
            thread_stats[tid].abort_unsafe++;
            break;

        case ABORT_REVERSE:
            thread_stats[tid].abort_reverse++;
			break;

        case ABORT_CONFLICT:
            thread_stats[tid].abort_conflict++;
            break;

        case ABORT_CACHEFULL:
            thread_stats[tid].abort_cachefull++;
            break;

        case ABORT_DEBUG:
            thread_stats[tid].abort_debug++;
            break;

        case ABORT_NESTED:
            thread_stats[tid].abort_nested++;
            break;

        case ABORT_GENERIC:
            thread_stats[tid].abort_generic++;
            break;

        case ABORT_RETRY:
            thread_stats[tid].abort_retry++;
            break;
        case EVENTS_FETCHED:
			events_fetched++;
			break;
		case T_BTW_EVT:
			tot_time_between_events += value;
			t_btw_evts = tot_time_between_events/events_fetched;
			break;

        case ABORT_TOTAL:
            thread_stats[tid].abort_total++;
            break;

        default:
            printf("Unrecognized stat type (%d)\n", type);
    }
}

unsigned long long get_time_of_an_event(){
	return thread_stats[tid].clock_safe/thread_stats[tid].events_safe;
}

double get_frac_htm_aborted(){
	return thread_stats[tid].abort_total/thread_stats[tid].events_htm;
}

void gather_statistics() {
    unsigned int i;

    for(i = 0; i < n_cores; i++) {
        system_stats.events_total += thread_stats[i].events_total;
        system_stats.events_safe += thread_stats[i].events_safe;
        system_stats.events_htm += thread_stats[i].events_htm;
        system_stats.events_stm += thread_stats[i].events_stm;

        system_stats.commits_htm += thread_stats[i].commits_htm;
        system_stats.commits_stm += thread_stats[i].commits_stm;

        system_stats.clock_safe += thread_stats[i].clock_safe;
        system_stats.clock_htm += thread_stats[i].clock_htm;
        system_stats.clock_stm += thread_stats[i].clock_stm;
        system_stats.clock_htm_throttle += thread_stats[i].clock_htm_throttle;
        system_stats.clock_stm_wait += thread_stats[i].clock_stm_wait;
        system_stats.clock_undo_event += thread_stats[i].clock_undo_event;

        system_stats.abort_total += thread_stats[i].abort_total;
        system_stats.abort_unsafe += thread_stats[i].abort_unsafe;
        system_stats.abort_reverse += thread_stats[i].abort_reverse;
        system_stats.abort_conflict += thread_stats[i].abort_conflict;
        system_stats.abort_cachefull += thread_stats[i].abort_cachefull;
        system_stats.abort_nested += thread_stats[i].abort_nested;
        system_stats.abort_debug += thread_stats[i].abort_debug;
//        system_stats.abort_generic += thread_stats[i].abort_generic;
        system_stats.abort_retry += thread_stats[i].abort_retry;
    }

		//non so se sia corrette definirle così, perchè non si prende veramente il tempo medio della durata
		//ma la stessa durata come se fosse spalmata su tutti gli eventi
//        system_stats.clock_safe /= (double)system_stats.events_safe;
//        system_stats.clock_htm /= (double)system_stats.events_htm;
//        system_stats.clock_stm /= (double)system_stats.events_stm;
//        system_stats.clock_htm_throttle /= (double)system_stats.events_htm;
//        system_stats.clock_stm_wait /= (double)system_stats.events_stm;

		if(system_stats.events_safe != 0) {
		        system_stats.clock_safe /= system_stats.events_safe;
		}

		if(system_stats.events_htm != 0) {
		        system_stats.clock_htm /= system_stats.events_htm;
				system_stats.clock_htm_throttle /= system_stats.commits_htm;
		}

		if(system_stats.events_stm != 0) {
		        system_stats.clock_stm /= system_stats.events_stm;
		        system_stats.clock_stm_wait /= system_stats.events_stm;
		}
}

void print_statistics() {

    gather_statistics();

    printf("Simulation final report:\n");
    printf("Thread: %d\tLP: %d\t\n", n_cores, n_prc_tot);
    //printf("Lookahead: %d", 0);
    printf("\n");

    printf("Total events....................................: %11u\n", system_stats.events_total);
    printf("Safe events.....................................: %11u (%.2f%%)\n", system_stats.events_safe, ((double)system_stats.events_safe / system_stats.events_total)*100);
    printf("HTM events......................................: %11u (%.2f%%)\n", system_stats.events_htm, ((double)system_stats.events_htm / system_stats.events_total)*100);
    printf("STM events......................................: %11u (%.2f%%)\n\n", system_stats.events_stm, ((double)system_stats.events_stm / system_stats.events_total)*100);

    unsigned int commits_total = system_stats.events_safe + system_stats.commits_htm + system_stats.commits_stm;
    
    printf("TOT committed...................................: %11u\n", commits_total);
    printf("HTM committed...................................: %11u (%.2f%%)\n", system_stats.commits_htm, ((double)system_stats.commits_htm / system_stats.events_htm)*100);
    printf("STM committed...................................: %11u (%.2f%%)\n\n", system_stats.commits_stm, ((double)system_stats.commits_stm / system_stats.events_stm)*100);

    printf("Average time spent in safe execution............: %llu clocks\n", system_stats.clock_safe);
    printf("Average time spent in HTM execution.............: %llu clocks\n", system_stats.clock_htm);
    printf("Average time spent for HTM throttling...........: %llu clocks (%.2f%%)\n", system_stats.clock_htm_throttle, ((double)system_stats.clock_htm_throttle / system_stats.clock_htm)*100);
    printf("Average time spent in STM execution.............: %llu clocks\n", system_stats.clock_stm);
    printf("Average time spent to be safe in STM execution..: %llu clocks (%.2f%%)\n", system_stats.clock_stm_wait, ((double)system_stats.clock_stm_wait / system_stats.clock_stm)*100);
    printf("Average time spent to reverse...................: %llu clocks\n", system_stats.clock_undo_event);
    printf("Average useful time spent in HTM execution......: %.2f clocks\n", system_stats.clock_htm/((double)system_stats.commits_htm / system_stats.events_htm));
    printf("Average useful time spent in STM execution......: %.2f clocks\n\n", system_stats.clock_stm/((double)system_stats.commits_stm / system_stats.events_stm));
    

    unsigned int abort_total = system_stats.abort_unsafe +
        system_stats.abort_conflict +
        system_stats.abort_cachefull +
        system_stats.abort_nested +
        system_stats.abort_debug;

    system_stats.abort_generic = system_stats.abort_total - abort_total;

    printf("Total HTM aborts................................: %11u (%.2f%%)\n", system_stats.abort_total,  ((double)system_stats.abort_total/(double)system_stats.events_htm)*100);

    printf("HTM aborts for UNSAFETY.........................: %11u (%.2f%%)\n", system_stats.abort_unsafe, ((double)system_stats.abort_unsafe / abort_total)*100);
    printf("HTM aborts for CONFLICT.........................: %11u (%.2f%%)\n", system_stats.abort_conflict, ((double)system_stats.abort_conflict / abort_total)*100);
    printf("HTM aborts for CACHEFULL........................: %11u (%.2f%%)\n", system_stats.abort_cachefull, ((double)system_stats.abort_cachefull / abort_total)*100);
    printf("HTM aborts for NESTED...........................: %11u (%.2f%%)\n", system_stats.abort_nested, ((double)system_stats.abort_nested / abort_total)*100);
    printf("HTM aborts for DEBUG............................: %11u (%.2f%%)\n", system_stats.abort_debug, ((double)system_stats.abort_debug / abort_total)*100);
    printf("HTM aborts for GENERIC..........................: %11u (%.2f%%)\n", system_stats.abort_generic, ((double)system_stats.abort_generic / abort_total)*100);
    printf("HTM abort RETRY.................................: %11u (%.2f%%)\n\n", system_stats.abort_retry, ((double)system_stats.abort_retry / abort_total)*100);

    printf("STM abort for UNSAFETY..........................: %11u\n\n", system_stats.abort_reverse);
    
#ifdef THROTTLING
    printf("Final delta.....................................: %.1f\n", delta_count);
#endif
#ifdef REVERSIBLE
    printf("Final threshold.................................: %u\n\n", reverse_execution_threshold);
#endif
}

