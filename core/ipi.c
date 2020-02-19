#if IPI_SUPPORT==1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>
#include <unistd.h>

#include <core.h>
#include <ipi_ctrl.h>
#include <preempt_counter.h>
#include <hpdcs_utils.h>
#include <queue.h>
#include <ipi.h>
#include <handle_interrupt_with_check.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/capability.h>

#include <lp_stats.h>

#define IPI_ARRIVAL_TIME 2000ULL//in clock cycles
#define FACTOR_IPI 2
#define TR (IPI_ARRIVAL_TIME*FACTOR_IPI)
#define BETA 1//greather or equals than 1
#define TR_PRIME (BETA*TR)


/* generate a random floating point number from min to max */
double randfrom(double min, double max) 
{
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

//used to debug sync_trampoline
 __thread unsigned long trampoline_count0 = 0;
 __thread unsigned long trampoline_count1 = 0;
 __thread unsigned long trampoline_count2 = 0;
 __thread unsigned long trampoline_count3 = 0;
 __thread unsigned long trampoline_count4 = 0;
 __thread unsigned long trampoline_count5 = 0;
 __thread unsigned long trampoline_count6 = 0;
 

__thread int ipi_registration_error = 0;

__thread void * alternate_stack = NULL;
__thread unsigned long alternate_stack_area = 4096UL;

__thread unsigned long interruptible_section_start = 0UL;
__thread unsigned long interruptible_section_end = 0UL;
char program_name[MAX_LEN_PROGRAM_NAME];

struct run_time_data rt_data;

#define SYSCALL_IPI_INDEX 134

static inline __attribute__((always_inline)) int ipi_syscall(unsigned int core_id)
{
    int ret = 0;
    asm volatile
    (
        "syscall"
        : "=a" (ret)
        : "0"(SYSCALL_IPI_INDEX), "D"(core_id)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void run_time_data_init (void)
{
  rt_data.in_lpstate_priority_message_offset = offsetof(struct _LP_state, priority_message);
  rt_data.in_lpstate_bound_offset = offsetof(struct _LP_state, bound);
  rt_data.in_msg_state_offset = offsetof(struct __msg_t, state);
  rt_data.in_msg_monitor_offset = offsetof(struct __msg_t, tie_breaker);
  rt_data.in_msg_timestamp_offset = offsetof(struct __msg_t, timestamp);
  rt_data.in_stats_ipi_trampoline_received_offset = offsetof(struct stats_t, ipi_trampoline_received_tid);
  rt_data.sizeof_stats = sizeof(struct stats_t);
}

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

void set_program_name(char*program_name_to_set){
    //copy program_name in variable program_name
    unsigned int len_string_arg0=strlen(program_name_to_set);
    if(len_string_arg0 >= MAX_LEN_PROGRAM_NAME){
        fprintf(stderr, "Program_name %s has more characters than %d\n", program_name_to_set,MAX_LEN_PROGRAM_NAME);
        gdb_abort;
    }
    memset(program_name,'\0',MAX_LEN_PROGRAM_NAME);
    memcpy(program_name,program_name_to_set,len_string_arg0);
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

void check_ipi_capability(){
    if(!enough_memory_lockable()){
        printf("not enough memory lockable\n");
        gdb_abort;
    }
    printf("program has ipi_capability\n");
}

static inline __attribute__((always_inline)) bool decision_model(LP_state *lp_ptr, msg_t *event_dest_in_execution){
    clock_timer avg_timer = (clock_timer) ((lp_evt_stats*)lp_ptr->lp_statistics)->lp_state[( (event_dest_in_execution->execution_mode!=LP_STATE_READY) ? 1 : 0)].evt_type[(unsigned int)event_dest_in_execution->type].avg_exec_time;
    if(avg_timer==NO_TIMER) //if avg_timer è 0 ossia non popolato manda ipi ossia no valore==no decision_model ==si manda ipi
        return false;
    
    if (avg_timer >= TR)
    {
        //possibile interruzione e riuso di event execution crea problemi??
        clock_timer executed_time = clock_timer_value(event_dest_in_execution->evt_start_time);
        if (executed_time < avg_timer)
            if ((avg_timer - executed_time) >= TR_PRIME)
                return true;
    }

    return false;
}

void send_ipi_to_lp(msg_t*event){
    //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
    unsigned int lck_tid;
    unsigned int lp_idx;
    unsigned long long syscall_time = 0;
    bool ipi_useful=false;
    lp_idx=event->receiver_id;
    lck_tid=(lp_lock[(lp_idx)*CACHE_LINE_SIZE/4]);
    if(lck_tid==0)
        return;
    msg_t*event_dest_in_execution = LPS[lp_idx]->msg_curr_executed;
    if(event_dest_in_execution!=NULL && event->timestamp < event_dest_in_execution->timestamp){
        ipi_useful = decision_model(LPS[lp_idx], event_dest_in_execution);
        if(ipi_useful){
            //this unpreemptable barrier can be relaxed to contains only correlated statistics,no need to protect instructions before "syscall",
            //but in this way the number of syscalls must be coherent with kernel module counters!!
            enter_in_unpreemptable_zone();

            #if REPORT==1
            clock_timer_start(syscall_time);
            #endif

            if (ipi_syscall(lck_tid-1)){
                printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
                gdb_abort;
            }

            #if REPORT==1 //protect correlated statistics
            statistics_post_th_data(tid,STAT_IPI_SYSCALL_TIME_TID,clock_timer_value(syscall_time));
            statistics_post_th_data(tid,STAT_IPI_SENDED_TID,1);

            #endif

            exit_from_unpreemptable_zone();
        }
    }
}

long get_sizeof_function(const char*function_name,char*path_program_name){
    //get size in byte of function_name present in executable program_name
    FILE *fp;
    char command[100];
    int index=0;

    //return values
    char func_target[50];
    char type[3];
    unsigned long addr;
    long function_size;

     //prepare command string
    memset(command,'\0',100);
    const char*basic_command="nm -S --size-sort -t d ";
    int len_basic=strlen(basic_command);
    memcpy(command,basic_command,len_basic);
    index+=len_basic;
    memcpy(command+index,path_program_name,strlen(path_program_name));
    index+=strlen(path_program_name);
    command[index]=' ';
    index+=1;
    memcpy(command+index,"| grep ",strlen("| grep "));
    index+=strlen("| grep ");
    memcpy(command+index,function_name,strlen(function_name));

    fp = popen(command, "r");
    if(fscanf(fp,"%lu %lu %s %s\n",&addr,&function_size,type,func_target)!=4){
        pclose(fp);
        printf("invalid parameter in function get_size_function\n");
        printf("%lu,%lu,%s,%s\n",addr,function_size,type,func_target);
        return -1;
    }
    if(strcmp(func_target,function_name)==0){
        pclose(fp);
        return function_size;
    }
    return -1;
}

void register_thread_to_ipi_module(unsigned int thread_id, const char* function_name, unsigned long address_function){
    long function_size=get_sizeof_function(function_name,program_name);
    if(function_size<0){
        printf("Impossible to retrieve function size of \n");
        gdb_abort;
    }
    #if VERBOSE>0
    else if(thread_id==0){
        printf("ProcessEvent has size=%ld\n",function_size);
    }
    #endif
    interruptible_section_start = address_function;
    interruptible_section_end = (address_function + function_size);//this is an address,if function has size func_size and start from start,then range [start,start+func_size] contains func_size+1 addresses
    if(thread_id==0)
        printf("Register thread,code_interruptible_start=%lu,code_interruptible_end=%lu\n",interruptible_section_start,interruptible_section_end);

    ipi_registration_error = ipi_register_thread(thread_id, (unsigned long)cfv_trampoline, &alternate_stack,
        &preempt_count_ptr,&standing_ipi_ptr,alternate_stack_area, interruptible_section_start, interruptible_section_end);
    if(ipi_registration_error!=0){
        printf("Impossible register_thread %d\n",thread_id);
        gdb_abort;
    }
    if(thread_id==0)
        printf("Thread %d finish registration to IPI module\n",thread_id);
    #if VERBOSE > 0
    else
        printf("Thread %d finish registration to IPI module\n",thread_id);
    #endif
}

void ipi_unregister(){
    ipi_unregister_thread(&alternate_stack, alternate_stack_area);
}

void ipi_print_and_reset_counters(){
    ipi_print_and_reset_counters_ipi_module();
}

#if DEBUG==1

void print_counters(const char*type,unsigned long*counters){
    printf("%s\n",type);
    for(int i=0;i<7;i++){
        printf("%lu ",counters[i]);
    }
    printf("\n");
    return;
}
void print_trampoline_counters(){
    printf("%lu %lu %lu %lu %lu %lu %lu\n",trampoline_count0,trampoline_count1,trampoline_count2,trampoline_count3,trampoline_count4,trampoline_count5,trampoline_count6);
}
void reset_all_trampoline_counters(){
    trampoline_count0 = 0;
    trampoline_count1 = 0;
    trampoline_count2 = 0;
    trampoline_count3 = 0;
    trampoline_count4 = 0;
    trampoline_count5 = 0;
    trampoline_count6 = 0;
    return;
}

void initialize_trampoline_counters(unsigned long*trampoline_counters){
    trampoline_counters[0]=trampoline_count0;
    trampoline_counters[1]=trampoline_count1;
    trampoline_counters[2]=trampoline_count2;
    trampoline_counters[3]=trampoline_count3;
    trampoline_counters[4]=trampoline_count4;
    trampoline_counters[5]=trampoline_count5;
    trampoline_counters[6]=trampoline_count6;
}

bool check_arrays_counters(unsigned long *expected_counters,unsigned long *trampoline_counters){
    for(unsigned int i=0;i<7;i++){
        if(expected_counters[i]!=trampoline_counters[i])
            return false;
    }
    return true;
}
void reset_counters(unsigned long*expected_counters){
    for(unsigned int i=0;i<7;i++){
        expected_counters[i]=0;
    }
}
void call_trampoline_and_check_arrays_counters(char*string_to_print_on_error,unsigned long*expected_counters,bool preemption_counter_incremented){
    sync_trampoline();
    unsigned long trampoline_counters[7]={0};
    initialize_trampoline_counters(trampoline_counters);
    unsigned long expected_preemption_counter=PREEMPT_COUNT_INIT;
    if(preemption_counter_incremented){
        expected_preemption_counter+=1;
    }
    if(!check_arrays_counters(expected_counters,trampoline_counters) || (thread_stats[0].ipi_trampoline_received_tid!=1) || get_preemption_counter()!=expected_preemption_counter){
        print_counters("expected",expected_counters);
        print_counters("result\n",trampoline_counters);
        printf("%s\n",string_to_print_on_error);
        gdb_abort;
    }
    thread_stats[0].ipi_trampoline_received_tid=0;
    reset_preemption_counter();
    reset_all_trampoline_counters();
}

void check_priority_msg_null(){
    //no one pre operation
    unsigned long expected_counters[7]={1,0,0,0,0,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null failed",expected_counters,false);
}

void check_priority_msg_not_null_state_extracted(msg_t*priority_message){
    priority_message->state=EXTRACTED;
    LPS[current_lp]->priority_message=priority_message;
    unsigned long expected_counters[7]={1,1,0,0,0,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null_state_extracted failed",expected_counters,false);
    LPS[current_lp]->priority_message=NULL;
}
void check_priority_msg_not_null_state_eliminated(msg_t*priority_message){
    priority_message->state=ELIMINATED;
    LPS[current_lp]->priority_message=priority_message;
    unsigned long expected_counters[7]={1,1,0,0,0,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null_state_eliminated failed",expected_counters,false);
    LPS[current_lp]->priority_message=NULL;
}
void check_priority_msg_not_null_state_anti_msg_banana(msg_t*priority_message){
    priority_message->state=ANTI_MSG;
    priority_message->monitor=(void*)0xBA4A4A;
    LPS[current_lp]->priority_message=priority_message;
    unsigned long expected_counters[7]={1,1,1,0,0,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null_state_anti_msg_banana failed",expected_counters,false);
    LPS[current_lp]->priority_message=NULL;
}

void check_priority_msg_not_null_state_anti_msg_not_banana(msg_t*priority_message){
    priority_message->state=ANTI_MSG;
    priority_message->monitor=(void*)0x0;
    LPS[current_lp]->priority_message=priority_message;
    unsigned long expected_counters[7]={1,1,1,0,0,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null_state_anti_msg_not_banana failed",expected_counters,false);
    LPS[current_lp]->priority_message=NULL;
}
void check_priority_msg_not_null_state_new_evt(msg_t*priority_message){
    priority_message->state=NEW_EVT;
    LPS[current_lp]->priority_message=priority_message;
    unsigned long expected_counters[7]={1,1,0,0,1,0,0};
    call_trampoline_and_check_arrays_counters("check_priority_msg_null_state_new_evt failed",expected_counters,false);
    LPS[current_lp]->priority_message=NULL;
}
void check_priority_msg_not_null_state_anti_msg_not_banana_bound_not_null(msg_t*priority_message,msg_t*bound){
    priority_message->state=ANTI_MSG;
    priority_message->monitor=(void*)0x0;
    LPS[current_lp]->priority_message=priority_message;
    LPS[current_lp]->bound=bound;
    unsigned long expected_counters[7]={0};
    bool preemption_counter_must_be_incremented=false;
    for(int i=0;i<10000;i++){
        reset_counters(expected_counters);
        double random_double1,random_double2;
        random_double1=randfrom(0.0, 10000.0);
        random_double2=randfrom(0.0, 10000.0);
        priority_message->timestamp=random_double1;
        bound->timestamp=random_double2;
        preemption_counter_must_be_incremented=false;
        if(priority_message->timestamp>=bound->timestamp){
            expected_counters[0]=1;
            expected_counters[1]=1;
            expected_counters[2]=1;
            expected_counters[5]=1;
            preemption_counter_must_be_incremented=false;
        }
        else{
            expected_counters[0]=1;
            expected_counters[1]=1;
            expected_counters[2]=1;
            expected_counters[5]=1;
            expected_counters[6]=1;
            preemption_counter_must_be_incremented=true;
        }
        call_trampoline_and_check_arrays_counters("check_priority_msg_not_null_state_anti_msg_not_banana_bound_not_null failed",expected_counters,preemption_counter_must_be_incremented);
    }
    LPS[current_lp]->priority_message=NULL;
    LPS[current_lp]->bound=NULL;
}

void check_priority_msg_not_null_state_new_evt_bound_not_null(msg_t*priority_message,msg_t*bound){
    priority_message->state=NEW_EVT;
    LPS[current_lp]->priority_message=priority_message;
    LPS[current_lp]->bound=bound;
    unsigned long expected_counters[7]={0};
    bool preemption_counter_must_be_incremented=false;

    for(int i=0;i<10000;i++){
        reset_counters(expected_counters);
        double random_double1,random_double2;
        random_double1=randfrom(0.0, 10000.0);
        random_double2=randfrom(0.0, 10000.0);
        priority_message->timestamp=random_double1;
        bound->timestamp=random_double2;
        preemption_counter_must_be_incremented=false;

        if(priority_message->timestamp>=bound->timestamp){
            expected_counters[0]=1;
            expected_counters[1]=1;
            expected_counters[4]=1;
            expected_counters[5]=1;
            preemption_counter_must_be_incremented=false;
        }
        else{
            expected_counters[0]=1;
            expected_counters[1]=1;
            expected_counters[4]=1;
            expected_counters[5]=1;
            expected_counters[6]=1;
            preemption_counter_must_be_incremented=true;
        }
        call_trampoline_and_check_arrays_counters("check_priority_msg_not_null_state_new_evt_bound_not_null failed",expected_counters,preemption_counter_must_be_incremented);
    }
    LPS[current_lp]->priority_message=NULL;
    LPS[current_lp]->bound=NULL;
}

void check_trampoline_function(){
    //bound is NULL,LPS[current_lp]->priority_msg==NULL
    if(LPS[current_lp]->bound!=NULL){
        printf("bound not null\n");
        gdb_abort;
    }
    if(LPS[current_lp]->priority_message!=NULL){
        printf("priority_msg not null\n");
        gdb_abort;
    }
    srand(time( NULL));//set initial seed

    reset_all_trampoline_counters();
    msg_t *evt1=allocate_event(current_lp);
    msg_t *evt2=allocate_event(current_lp);

    check_priority_msg_null();

    check_priority_msg_not_null_state_extracted(evt1);
    check_priority_msg_not_null_state_eliminated(evt1);

    check_priority_msg_not_null_state_anti_msg_banana(evt1);
    check_priority_msg_not_null_state_anti_msg_not_banana(evt1);

    check_priority_msg_not_null_state_new_evt(evt1);

    check_priority_msg_not_null_state_anti_msg_not_banana_bound_not_null(evt1,evt2);

    check_priority_msg_not_null_state_new_evt_bound_not_null(evt1,evt2);

    reset_all_trampoline_counters();

    LPS[current_lp]->priority_message=NULL;
    LPS[current_lp]->bound=NULL;

    deallocate_event(evt1);
    deallocate_event(evt2);

    //in case this check will be modified remember to delete all side-effects created with calls to sync_trampoline(), or with the setting of bound,priority_msg ecc
}

#endif

#endif
