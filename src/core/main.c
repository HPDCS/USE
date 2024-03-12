#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <argp.h> 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <core.h>
#include <timer.h>
#include <hpdcs_utils.h>
#include <reverse.h>
#include <statistics.h>

#include <incremental_state_saving.h>

__thread struct drand48_data seedT;

unsigned long long tid_ticket = 1;
int device_fd;

//extern double delta_count;
extern int reverse_execution_threshold;

clock_timer simulation_clocks;
timer exec_time;

void *start_thread(){
	
	int tid = (int) __sync_fetch_and_add(&tid_ticket, 1);
	
    srand48_r(tid+254, &seedT);

	//START THREAD (definita in core.c)
	thread_loop(tid);

	pthread_exit(NULL);

}

void start_simulation() {

    pthread_t p_tid[pdes_config.ncores-1];
    int ret;
    unsigned int i;

    /// open device file 
    if (pdes_config.checkpointing == INCREMENTAL_STATE_SAVING && !pdes_config.iss_signal_mprotect) {
        device_fd = open("/dev/tracker", (O_RDONLY | O_NONBLOCK));
        if (device_fd == -1) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
        ioctl(device_fd, TRACKER_SET_SEGSIZE, PER_LP_PREALLOCATED_MEMORY);
    }

    //Child threads
    for(i = 0; i < pdes_config.ncores - 1; i++) {
        if( (ret = pthread_create(&p_tid[i], NULL, start_thread, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }

    //Main thread
    thread_loop(0);

    for(i = 0; i < pdes_config.ncores-1; i++){
        pthread_join(p_tid[i], NULL);
    }
}





int main(int argn, char *argv[]) {
    
    parse_options(argn, argv);
    print_config();

    printf("***START SIMULATION***\n\n");



	start_simulation();
    double simduration = (double)timer_value_seconds(exec_time);

    print_statistics();

    printf("Simulation ended (seconds): %12.2f\n", simduration);
    printf("Simulation ended  (clocks): %llu\n", clock_timer_value(simulation_clocks));
    printf("Last gvt: %f\n", current_lvt);
    printf("EventsPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration);
    printf("EventsPerThreadPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration/pdes_config.ncores);
    statistics_fini();

    return 0;
}

