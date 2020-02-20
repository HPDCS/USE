#include <stdio.h>
#include <stdlib.h>
#include <preempt_counter.h>
#include <hpdcs_utils.h>

#if PREEMPT_COUNTER==1
//this function is module private
void set_preemption_counter(unsigned long value){
	*preempt_count_ptr=value;
}

#if IPI_SUPPORT==1
__thread unsigned long long * preempt_count_ptr = NULL;
__thread unsigned long long * standing_ipi_ptr = NULL;
	void initialize_preempt_counter(){
		set_preemption_counter(PREEMPT_COUNT_INIT);//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
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
		set_preemption_counter(PREEMPT_COUNT_INIT);//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}

	void initialize_standing_ipi(){
		standing_ipi_ptr=&standing_ipi;
		*standing_ipi_ptr=STANDING_IPI_INIT;//counter>=1 means NOT_INTERRUPTIBLE,counter==0 means INTERRUPTIBLE
	}
#endif

unsigned long get_preemption_counter(){
	return *preempt_count_ptr;
}

void reset_preemption_counter(){
	set_preemption_counter(PREEMPT_COUNT_INIT);
}
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
	//this barrier before decrement follows implementation of preempt_enable() function in the linux kernel v 5.0
	__asm__ __volatile__("": : :"memory");
	__sync_sub_and_fetch(preempt_count_ptr, 1);
}

void increment_preempt_counter(){
	#if DEBUG==1
	if((*preempt_count_ptr)>=MAX_PREEMPT_COUNTER){
		printf("preempt_count is equals to MAX_PREEMPT_COUNTER %d in increment_preempt_counter\n",MAX_PREEMPT_COUNTER);
		gdb_abort;
	}
	if(*preempt_count_ptr==(unsigned long long)INVALID_PREEMPT_COUNTER){
			printf("invalid preempt counter\n");
			gdb_abort;
	}
	#endif
	__sync_add_and_fetch(preempt_count_ptr, 1);
	//this barrier after increment follows implementation of preempt_disable() function in the linux kernel v 5.0
	__asm__ __volatile__("": : :"memory");
}

#endif //PREEMPT_COUNTER