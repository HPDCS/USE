#if IPI_SUPPORT==1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <statistics.h>
#include <timer.h>
#include <unistd.h>

#include <core.h>
#include <ipi.h>
#include <ipi_ctrl.h>
#include <preempt_counter.h>
#include <hpdcs_utils.h>
#include <queue.h>
#include <ipi.h>
__thread int ipi_registration_error = 0;

__thread void * alternate_stack = NULL;
__thread unsigned long alternate_stack_area = 4096UL;

__thread unsigned long interruptible_section_start = 0UL;
__thread unsigned long interruptible_section_end = 0UL;
char program_name[MAX_LEN_PROGRAM_NAME];

struct run_time_data rt_data;

void run_time_data_init (void)
{
  rt_data.in_lpstate_priority_message_offset = offsetof(struct _LP_state, priority_message);
  rt_data.in_lpstate_bound_offset = offsetof(struct _LP_state, bound);
  rt_data.in_msg_state_offset = offsetof(struct __msg_t, state);
  rt_data.in_msg_monitor_offset = offsetof(struct __msg_t, tie_breaker);
  rt_data.in_msg_timestamp_offset = offsetof(struct __msg_t, timestamp);
  rt_data.in_stats_ipi_trampoline_received_offset = offsetof(struct stats_t, ipi_trampoline_received);
  rt_data.sizeof_stats = sizeof(struct stats_t);
}

void send_ipi_to_lp(msg_t*event){
    //lp is locked by thread tid if lp_lock[lp_id]==tid+1,else lp_lock[lp_id]=0
    unsigned int lck_tid;
    unsigned int lp_idx;
    unsigned long long user_time = 0;
	clock_timer_start(user_time);
    lp_idx=event->receiver_id;
    lck_tid=(lp_lock[(lp_idx)*CACHE_LINE_SIZE/4]);
    if(lck_tid==0)
        return;
    msg_t*event_dest_in_execution = LPS[lp_idx]->msg_curr_executed;
    if(event_dest_in_execution!=NULL && event->timestamp < event_dest_in_execution->timestamp){
        #if REPORT==1
        statistics_post_th_data(tid,STAT_IPI_SENDED,1);
		statistics_post_th_data(tid,STAT_IPI_SYSCALL_TIME,clock_timer_value(user_time));
        #endif
        if (syscall(174, lck_tid-1))
            printf("[IPI_4_USE] - Syscall to send IPI has failed!!!\n");
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
void register_thread_to_ipi_module(unsigned int thread_id,const char* function_name,unsigned long address_function){
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
    interruptible_section_end = address_function+function_size-1;
    if(thread_id==0)
        printf("Register thread,code_interruptible_start=%lu,code_interruptible_end=%lu\n",interruptible_section_start,interruptible_section_end);

    //mettere cfv_trampoline al posto di ProcessEvent
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
#endif
