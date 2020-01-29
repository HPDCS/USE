#ifndef IPI_H
#define IPI_H

void send_ipi_to_lp(msg_t*event);
void ipi_unregister();
void register_thread_to_ipi_module(unsigned int thread_id,const char* function_name,unsigned long address_function);
extern void cfv_trampoline(void);
#define MAX_LEN_PROGRAM_NAME 64 //this len includes string terminator

extern __thread void * alternate_stack;
extern __thread unsigned long alternate_stack_area;
#endif