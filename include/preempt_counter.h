#ifndef PREEMPT_COUNTER_H
#define PREEMPT_COUNTER_H

#if PREEMPT_COUNTER==1
#define PREEMPT_COUNT_CODE_INTERRUPTIBLE 0
#define PREEMPT_COUNT_CODE_NOT_INTERRUPTIBLE (*preempt_count_ptr>PREEMPT_COUNT_CODE_INTERRUPTIBLE)

#define PREEMPT_COUNT_INIT_CODE_INTERRUPTIBLE 0
#define PREEMPT_COUNT_INIT_CODE_NOT_INTERRUPTIBLE 1

#define PREEMPT_COUNT_INIT PREEMPT_COUNT_INIT_CODE_NOT_INTERRUPTIBLE
#define INVALID_PREEMPT_COUNTER (-1)

#define MAX_DEFAULT_PREEMPT_COUNTER 2

#if ONGVT_PERIOD!=-1
	#define MAX_PREEMPT_COUNTER (MAX_DEFAULT_PREEMPT_COUNTER+1)
#else
	#define MAX_PREEMPT_COUNTER (MAX_DEFAULT_PREEMPT_COUNTER)
#endif

#define STANDING_IPI_ZERO 0
#define STANDING_IPI_ONE 1
#define STANDING_IPI_INIT STANDING_IPI_ZERO
#define INVALID_STANDING_IPI (-1)
#endif

void initialize_preempt_counter();
void initialize_standing_ipi();
unsigned long increment_preempt_counter();
unsigned long decrement_preempt_counter();
unsigned long get_preemption_counter();

#define	print_preemption_counter(thread_id) printf("preempt_counter=%llu,tid=%d\n",get_preemption_counter(),thread_id)


extern __thread unsigned long long * preempt_count_ptr;
extern __thread unsigned long long * standing_ipi_ptr;

#endif

