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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>

extern bool sim_error;
extern volatile bool stop_timer;
extern volatile bool stop;

#if IPI_SUPPORT==1
#include <ipi.h>
#endif

#if DECISION_MODEL==1 && REPORT==1
#include <lp_stats.h>
#endif

#ifndef MAX_MEMORY_ALLOCABLE
#define GIGA (1024ULL*1024ULL*1024ULL)
#define MAX_MEMORY_ALLOCABLE (200ULL*GIGA)
#endif

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
//#if SPERIMENTAL == 1
//    printf("\t- SPERIMENTAL features enabled.\n");
//#endif
//#if PREEMPTIVE == 1
//    printf("\t- PREEMPTIVE event realease enabled.\n");
//#endif
#if DEBUG == 1
    printf("\t- DEBUG mode enabled.\n");
#endif
    printf("\t- DYMELOR enabled.\n");
    printf("\t- CACHELINESIZE %u\n", CACHE_LINE_SIZE);
    printf("\t- CHECKPOINT PERIOD %u\n", CHECKPOINT_PERIOD);
    printf("\t- EVTS/LP BEFORE CLEAN CKP %u\n", CLEAN_CKP_INTERVAL);
    printf("\t- ON_GVT PERIOD %u\n", ONGVT_PERIOD);
#if REPORT == 1
    printf("\t- REPORT prints enabled.\n");
#endif

#if CONSTANT_CHILD_INVALIDATION==1
    printf("\t- CONSTANT_CHILD_INVALIDATION enabled.\n");
#endif
#if PREEMPT_COUNTER==1
    printf("\t- PREEMPT_COUNTER enabled.\n");
#endif
#if LONG_JMP==1
    printf("\t- LONG_JMP enabled.\n");
#endif
#if HANDLE_INTERRUPT==1
    printf("\t- HANDLE_INTERRUPT enabled.\n");
#endif
#if HANDLE_INTERRUPT_WITH_CHECK==1
    printf("\t- HANDLE_INTERRUPT_WITH_CHECK enabled.\n");
#endif
#if POSTING==1
    printf("\t- POSTING enabled.\n");
#endif

#if IPI_SUPPORT==1
    printf("\t- IPI_SUPPORT enabled.\n");
#endif

#if SYNCH_CHECK==1
    printf("\t- SYNCH_CHECK enabled.\n");
#endif

#if DECISION_MODEL==1
    printf("\t- DECISION_MODEL enabled.\n");
#endif

//#if REVERSIBLE == 1
//    printf("\t- SPECULATIVE SIMULATION\n");
//#else
//    printf("\t- CONSERVATIVE SIMULATION\n");
//#endif
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
void set_max_memory_allocable(){
    struct rlimit set_memory_limit;
    set_memory_limit.rlim_cur=MAX_MEMORY_ALLOCABLE;
    set_memory_limit.rlim_max=MAX_MEMORY_ALLOCABLE;
    
    if(setrlimit(RLIMIT_AS,&set_memory_limit)!=0){
        printf("errore set limit address space\n");
        perror("error:");
        gdb_abort;
    }
}

#if DEBUG==1
void test_memory_limit_mmap(unsigned long byte_to_alloc){
    char*memory_test;
    if ((memory_test = mmap(NULL, (size_t) byte_to_alloc, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0)) == MAP_FAILED){
        printf("unable to alloc memory with mmap,test succeed\n");
        perror("error");
    }
    else
        free(memory_test);//free memory if limit not exceed
}


void test_memory_limit_malloc(unsigned long byte_to_alloc){
    char*memory_test=malloc(byte_to_alloc);
    if(memory_test==NULL){
        printf("unable to alloc memory with malloc,test succeed\n");
        perror("error");
    }
    else
        free(memory_test);//free memory if limit not exceed
}
#endif

int main(int argn, char *argv[]) {
    timer exec_time;

    if(argn < 3) {
        fprintf(stderr, "Usage: %s: n_threads n_lps\n", argv[0]);
        exit(EXIT_FAILURE);

    } else {
        n_cores = atoi(argv[1]);
        n_prc_tot = atoi(argv[2]);
        if(argn == 4)
            sec_stop = atoi(argv[3]);
    }
  
#if IPI_SUPPORT==1
    set_program_name(argv[0]);
    check_ipi_capability();
#endif

    set_max_memory_allocable();
    #if DEBUG==1
    test_memory_limit_malloc(MAX_MEMORY_ALLOCABLE);
    test_memory_limit_mmap(MAX_MEMORY_ALLOCABLE);
    #endif
    printf("***START SIMULATION***\n\n");

    timer_start(exec_time);

	clock_timer simulation_clocks;
	clock_timer_start(simulation_clocks);

	start_simulation();
    simduration = (double)timer_value_seconds(exec_time);

    print_statistics();
    
    #if DECISION_MODEL==1 && REPORT==1
    fini_lp_stats(LPS, n_prc_tot);
    #endif

    printf("Simulation ended (seconds): %12.2f\n", simduration);
    printf("Simulation ended  (clocks): %llu\n", clock_timer_value(simulation_clocks));
    printf("Last gvt: %f\n", current_lvt);
    printf("EventsPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration);
    printf("EventsPerThreadPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration/n_cores);
    
    #if REPORT==1
    write_results_on_csv("results/results.csv");//not present in original version
    #endif
    
    //statistics_fini();
    if(sim_error){
        printf(RED("Execution ended for an error\n"));
    } 
    else if (stop || stop_timer){
        //sleep(5);
        printf(GREEN( "Execution ended correctly\n"));
    }
    return 0;
}

