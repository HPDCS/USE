#ifndef IPI_H
#define IPI_H

void send_ipi_to_lp(msg_t*event);
void ipi_unregister();
void register_thread_to_ipi_module(unsigned int thread_id,const char* function_name,unsigned long address_function);
extern void cfv_trampoline(void);
#define MAX_LEN_PROGRAM_NAME 64 //this len includes string terminator

extern __thread void * alternate_stack;
extern __thread unsigned long alternate_stack_area;

struct run_time_data
{
  /* "0" */
  size_t in_lpstate_priority_message_offset;
  /* "1" */
  size_t in_lpstate_bound_offset;
  /* "2" */
  size_t in_msg_state_offset;
  /* "3" */
  size_t in_msg_monitor_offset;
  /* "4" */
  size_t in_msg_timestamp_offset;
  /* "5" */
  size_t in_stats_ipi_trampoline_received_offset;
  /* "6" */
  size_t sizeof_stats;
} __attribute__((packed,aligned(8)));

void run_time_data_init(void);
#endif