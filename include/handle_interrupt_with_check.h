#if HANDLE_INTERRUPT_WITH_CHECK==1

#ifndef HANDLE_INTERRUPT_CHECK_H
#define HANDLE_INTERRUPT_CHECK_H
#include <handle_interrupt.h>

#define MAX_NESTING_ZONE_PREEMPTABLE 1
#define MAX_NESTING_ZONE_UNPREEMPTABLE (MAX_PREEMPT_COUNTER-PREEMPT_COUNT_INIT)
#define NO_NESTING 0

#if DEBUG==1
extern __thread unsigned int nesting_zone_preemptable;
extern __thread unsigned int nesting_zone_unpreemptable;
void reset_nesting_counters();
#endif

void*default_handler(void*arg);

void enter_in_unpreemptable_zone();
void exit_from_unpreemptable_zone(void* (*handler)(void*arg_handler),void*arg_handler);

void enter_in_preemptable_zone(void* (*handler)(void*arg_handler),void*arg_handler);
void exit_from_preemptable_zone();

#if DEBUG==1
void check_preemptability();
#endif

void check_unpreemptability();
#endif

#endif