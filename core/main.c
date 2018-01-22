#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>


#include <core.h>
#include <timer.h>
#include <hpdcs_utils.h>
#include <reverse.h>
#include <statistics.h>

__thread struct drand48_data seedT;

unsigned long long tid_ticket = 1;

//extern double delta_count;
extern int reverse_execution_threshold;

void *start_thread(){
	
	int tid = (int) __sync_fetch_and_add(&tid_ticket, 1);
	
    srand48_r(tid+254, &seedT);

	//START THREAD (definita in core.c)
	thread_loop(tid);

	pthread_exit(NULL);

}

void start_simulation() {

    pthread_t p_tid[n_cores-1];//pthread_t p_tid[number_of_threads];//
    int ret;
    unsigned int i;


    printf(COLOR_CYAN "\nStarting an execution with %u THREADs, %u LPs :\n", n_cores, n_prc_tot);
#if SPERIMENTAL == 1
    printf("\t- SPERIMENTAL features enabled.\n");
#endif
#if PREEMPTIVE == 1
    printf("\t- PREEMPTIVE event realease enabled.\n");
#endif
#if DEBUG == 1
    printf("\t- DEBUG mode enabled.\n");
#endif
    printf("\t- DYMELOR enabled.\n");
    printf("\t- CACHELINESIZE %u\n", CACHE_LINE_SIZE);
#if REPORT == 1
    printf("\t- REPORT prints enabled.\n");
#endif
#if REVERSIBLE == 1
    printf("\t- SPECULATIVE SIMULATION\n");
#else
    printf("\t- CONSERVATIVE SIMULATION\n");
#endif
    printf("\n" COLOR_RESET);

    //Child thread
    for(i = 0; i < n_cores - 1; i++) {
        if( (ret = pthread_create(&p_tid[i], NULL, start_thread, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }

    //Main thread
    thread_loop(0);

    for(i = 0; i < n_cores-1; i++){
        pthread_join(p_tid[i], NULL);
    }
}

int main(int argn, char *argv[]) {
    timer exec_time;

    if(argn < 3) {
        fprintf(stderr, "Usage: %s: n_threads n_lps\n", argv[0]);
        exit(EXIT_FAILURE);

    } else {
        n_cores = atoi(argv[1]);
        n_prc_tot = atoi(argv[2]);
    }
  
    printf("***START SIMULATION***\n\n");

    timer_start(exec_time);

	clock_timer simulation_clocks;
	clock_timer_start(simulation_clocks);

	start_simulation();

    printf("Simulation ended (seconds): %.3f\n", (double)timer_value_seconds(exec_time));
    printf("Simulation ended  (clocks): %llu\n", clock_timer_value(simulation_clocks));


    print_statistics();

    //statistics_fini();

    return 0;
}

