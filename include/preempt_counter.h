#ifndef PREEMPT_COUNTER_H
#define PREEMPT_COUNTER_H

#if PREEMPT_COUNTER==1
#define NO_NESTING 0
#define MAX_NESTING_PREEMPTABLE_ZONE 1
#define MAX_NESTING_UNPREEMPTABLE_ZONE 1

#define PREEMPT_COUNT_CODE_INTERRUPTIBLE 0
#define PREEMPT_COUNT_CODE_NOT_INTERRUPTIBLE (*preempt_count_ptr>PREEMPT_COUNT_CODE_INTERRUPTIBLE)

#define PREEMPT_COUNT_INIT_CODE_INTERRUPTIBLE 0
#define PREEMPT_COUNT_INIT_CODE_NOT_INTERRUPTIBLE 1

#define PREEMPT_COUNT_INIT (PREEMPT_COUNT_INIT_CODE_NOT_INTERRUPTIBLE)
#define INVALID_PREEMPT_COUNTER (-1)

#if INTERRUPT_SILENT!=1
	#define MAX_NESTING_DEFAULT_PREEMPT_COUNTER 3
#else
	#define MAX_NESTING_DEFAULT_PREEMPT_COUNTER 10
#endif

#if ONGVT_PERIOD!=-1
	#define MAX_NESTING_PREEMPT_COUNTER (MAX_NESTING_DEFAULT_PREEMPT_COUNTER+1)
#else
	#define MAX_NESTING_PREEMPT_COUNTER (MAX_NESTING_DEFAULT_PREEMPT_COUNTER)
#endif

#define STANDING_IPI_ZERO 0
#define STANDING_IPI_ONE 1
#define STANDING_IPI_INIT STANDING_IPI_ZERO
#define INVALID_STANDING_IPI (-1)
#endif

void initialize_preemptability();
void initialize_preempt_counter();
void initialize_standing_ipi();
void set_default_preemptability();
unsigned long increment_preempt_counter();
unsigned long decrement_preempt_counter();
unsigned long enter_in_preemptable_zone();
unsigned long exit_from_preemptable_zone();
unsigned long enter_in_unpreemptable_zone();
unsigned long exit_from_unpreemptable_zone();

#if DEBUG==1
void reset_variables_nesting_zone();
#endif

extern __thread unsigned long long * preempt_count_ptr;
extern __thread unsigned long long * standing_ipi_ptr;

#if DEBUG==1
extern __thread unsigned long nesting_zone_preemptable;
extern __thread unsigned long nesting_zone_unpreemptable;
#endif

#endif