#include <stdio.h>
#include <stdlib.h>
#include <preempt_counter.h>
#include <hpdcs_utils.h>

#if PREEMPT_COUNTER==1

#if DEBUG==1
__thread unsigned long nesting_zone_preemptable=0;
__thread unsigned long nesting_zone_unpreemptable=0;
#endif

#define	print_preemption_counter(thread_id) printf("preempt_counter=%llu,tid=%d\n",*preempt_count_ptr,thread_id)

__thread unsigned long long * preempt_count_ptr = NULL;
__thread unsigned long long * standing_ipi_ptr = NULL;

void set_default_preemptability(){
	*preempt_count_ptr=PREEMPT_COUNT_INIT;
}

#if IPI_SUPPORT==1

	void initialize_preemptability(){
		set_default_preemptability();
	}
	void initialize_standing_ipi(){
		*standing_ipi_ptr=STANDING_IPI_INIT;
	}
#else
	__thread unsigned long long preempt_count = INVALID_PREEMPT_COUNTER;
	__thread unsigned long long standing_ipi = INVALID_STANDING_IPI;

	void initialize_preemptability(){
		preempt_count_ptr=&preempt_count;
		set_default_preemptability();
	}

	void initialize_standing_ipi(){
		standing_ipi_ptr=&standing_ipi;
		*standing_ipi_ptr=STANDING_IPI_INIT;
	}
#endif

unsigned long decrement_preempt_counter(){
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
	return __sync_sub_and_fetch(preempt_count_ptr, 1);
}

unsigned long increment_preempt_counter(){
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
	return __sync_add_and_fetch(preempt_count_ptr, 1);
}
unsigned long enter_in_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable>=MAX_NESTING_PREEMPTABLE_ZONE){
		printf("nesting preemptable zone unfeasible on enter,max_nesting=%d\n",MAX_NESTING_PREEMPTABLE_ZONE);
		gdb_abort;
	}
	nesting_zone_preemptable++;
	#endif
	return decrement_preempt_counter();
}
unsigned long exit_from_preemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_preemptable==NO_NESTING){
		printf("nesting preemptable zone unfeasible on exit\n");
		gdb_abort;
	}
	nesting_zone_preemptable--;
	#endif
	return increment_preempt_counter();
}

unsigned long enter_in_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable>=MAX_NESTING_UNPREEMPTABLE_ZONE){
		printf("nesting unpreemptable zone unfeasible on enter,max_nesting=%d\n",MAX_NESTING_UNPREEMPTABLE_ZONE);
		gdb_abort;
	}
	nesting_zone_unpreemptable++;
	#endif
	return increment_preempt_counter();
}

unsigned long exit_from_unpreemptable_zone(){
	#if DEBUG==1
	if(nesting_zone_unpreemptable==NO_NESTING){
		printf("nesting unpreemptable zone unfeasible on exit\n");
		gdb_abort;
	}
	nesting_zone_unpreemptable--;
	#endif
	return decrement_preempt_counter();
}
#if DEBUG==1
void reset_variables_nesting_zone(){
	nesting_zone_unpreemptable=0;
	nesting_zone_preemptable=0;
}
#endif

#endif //PREEMPT_COUNTER