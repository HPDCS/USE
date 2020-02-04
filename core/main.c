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

extern bool sim_error;
extern volatile bool stop_timer;
extern volatile bool stop;

#if IPI_SUPPORT==1
#include <ipi.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/capability.h>

extern char program_name[MAX_LEN_PROGRAM_NAME];
#define BYTES_TO_LOCK_PER_THREAD 4096
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
#if IPI_SUPPORT==1
unsigned long get_max_bytes_lockable(){
    struct rlimit curr_limit;
    if(getrlimit(RLIMIT_MEMLOCK,&curr_limit)!=0){
        printf("getrlimit failed\n");
        perror("error:");
        gdb_abort;
    }
    return curr_limit.rlim_cur;
}

bool pid_has_capability(pid_t pid,unsigned int type_capability,const char*cap_name){
    //type_capability=={CAP_EFFECTIVE,CAP_PERMITTED,CAP_INHERITABLE}
    cap_t cap;
    cap_value_t cap_value;
    cap_flag_value_t cap_flags_value;

    cap = cap_get_pid(pid);
    if (cap == NULL) {
        perror("cap_get_pid");
        exit(-1);
    }
    cap_value = CAP_CHOWN;
    if (cap_set_flag(cap, CAP_EFFECTIVE, 1, &cap_value, CAP_SET) == -1) {
        perror("cap_set_flag cap_chown");
        cap_free(cap);
        exit(-1);
    }
    
    /* permitted cap */
    cap_value = CAP_MAC_ADMIN;
    if (cap_set_flag(cap, CAP_PERMITTED, 1, &cap_value, CAP_SET) == -1) {
        perror("cap_set_flag cap_mac_admin");
        cap_free(cap);
        exit(-1);
    }

    /* inherit cap */
    cap_value = CAP_SETFCAP;
    if (cap_set_flag(cap, CAP_INHERITABLE, 1, &cap_value, CAP_SET) == -1) {
        perror("cap_set_flag cap_setfcap");
        cap_free(cap);
        exit(-1);
    }

    cap_from_name(cap_name, &cap_value);
    cap_get_flag(cap, cap_value, type_capability, &cap_flags_value);
    cap_free(cap);
    if (cap_flags_value == CAP_SET)
        return true;
    return false;
}

bool enough_memory_lockable(){
    bool cap_ipc_lock = pid_has_capability(getpid(),CAP_EFFECTIVE,"cap_ipc_lock");
    if(cap_ipc_lock)
        return true;
    unsigned long max_bytes_lockable=get_max_bytes_lockable();
    if(max_bytes_lockable>=n_cores*BYTES_TO_LOCK_PER_THREAD)
        return true;
    return false;
}

#endif
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
#if INTERRUPT_SILENT==1
    printf("\t- INTERRUPT_SILENT enabled.\n");
#endif
#if INTERRUPT_FORWARD==1
    printf("\t- INTERRUPT_FORWARD enabled.\n");
#endif
#if LONG_JMP==1
    printf("\t- LONG_JMP enabled.\n");
#endif
#if HANDLE_INTERRUPT==1
    printf("\t- HANDLE_INTERRUPT enabled.\n");
#endif
#if POSTING==1
    printf("\t- POSTING enabled.\n");
#endif
#if POSTING_SYNC_CHECK_SILENT==1
    printf("\t- POSTING_SYNC_CHECK_SILENT enabled.\n");
#endif
#if POSTING_SYNC_CHECK_FORWARD==1
    printf("\t- POSTING_SYNC_CHECK_FORWARD enabled.\n");
#endif
#if IPI_SUPPORT==1
    printf("\t- IPI_SUPPORT enabled.\n");
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
    //copy program_name in variable program_name
    unsigned int len_string_arg0=strlen(argv[0]);
    if(len_string_arg0 >= MAX_LEN_PROGRAM_NAME){
        fprintf(stderr, "Program_name %s has more characters than %d\n", argv[0],MAX_LEN_PROGRAM_NAME);
        exit(EXIT_FAILURE);
    }
    memset(program_name,'\0',MAX_LEN_PROGRAM_NAME);
    memcpy(program_name,argv[0],len_string_arg0);
    if(!enough_memory_lockable()){
        printf("not enough memory lockable\n");
        gdb_abort;
    }
#endif
    printf("***START SIMULATION***\n\n");

    timer_start(exec_time);

	clock_timer simulation_clocks;
	clock_timer_start(simulation_clocks);

	start_simulation();
    double simduration = (double)timer_value_seconds(exec_time);

    print_statistics();

    printf("Simulation ended (seconds): %12.2f\n", simduration);
    printf("Simulation ended  (clocks): %llu\n", clock_timer_value(simulation_clocks));
    printf("Last gvt: %f\n", current_lvt);
    printf("EventsPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration);
    printf("EventsPerThreadPerSec: %12.2f\n", ((double)system_stats->events_committed)/simduration/n_cores);
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

