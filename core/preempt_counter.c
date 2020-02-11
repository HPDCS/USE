#include <stdio.h>
#include <stdlib.h>
#include <preempt_counter.h>
#include <hpdcs_utils.h>

#if PREEMPT_COUNTER==1
#define	print_preemption_counter(thread_id) printf("preempt_counter=%llu,tid=%d\n",*preempt_count_ptr,thread_id)
#if IPI_SUPPORT==1
__thread unsigned long long * preempt_count_ptr = NULL;
__thread unsigned long long * standing_ipi_ptr = NULL;
	void initialize_preempt_counter(){
		*preempt_count_ptr=PREEMPT_COUNT_INIT;//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}
	void initialize_standing_ipi(){
		*standing_ipi_ptr=STANDING_IPI_INIT;//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}
#else
	__thread unsigned long long preempt_count = INVALID_PREEMPT_COUNTER;
	__thread unsigned long long * preempt_count_ptr = NULL;

	__thread unsigned long long standing_ipi = INVALID_STANDING_IPI;
	__thread unsigned long long * standing_ipi_ptr = NULL;

	void initialize_preempt_counter(){
		preempt_count_ptr=&preempt_count;
		*preempt_count_ptr=PREEMPT_COUNT_INIT;//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}

	void initialize_standing_ipi(){
		standing_ipi_ptr=&standing_ipi;
		*standing_ipi_ptr=STANDING_IPI_INIT;//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}
#endif

void decrement_preempt_counter(){
	#if DEBUG==1
	if((*preempt_count_ptr)==PREEMPT_COUNT_CODE_INTERRUPTIBLE){
		printf("impossible decrement preempt_counter,it is already zero\n");
		gdb_abort;
	}
	if(*preempt_count_ptr==(unsigned long long)INVALID_PREEMPT_COUNTER){
			printf("invalid preempt counter\n");
			gdb_abort;
	}
	#endif
	__sync_sub_and_fetch(preempt_count_ptr, 1);
}

void increment_preempt_counter(){
	#if DEBUG==1
	if((*preempt_count_ptr)>=MAX_NESTING_PREEMPT_COUNTER){
		printf("preempt_count is equals to MAX_NESTING %d in increment_preempt_counter\n",MAX_NESTING_PREEMPT_COUNTER);
		gdb_abort;
	}
	if(*preempt_count_ptr==(unsigned long long)INVALID_PREEMPT_COUNTER){
			printf("invalid preempt counter\n");
			gdb_abort;
	}
	#endif
	__sync_add_and_fetch(preempt_count_ptr, 1);
}

#endif //PREEMPT_COUNTER