#ifndef IPI_H
#define IPI_H

void send_ipi_to_lp(msg_t*event);
void register_thread_to_ipi_module(unsigned int thread_id,const char* function_name,unsigned long address_function);
extern void cfv_trampoline(void);
#define MAX_LEN_PROGRAM_NAME 64 //this len includes string terminator
#endif