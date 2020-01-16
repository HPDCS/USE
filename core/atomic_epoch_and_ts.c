#include <atomic_epoch_and_ts.h>
#include <stdio.h>

atomic_epoch_and_ts atomic_load_epoch_and_ts(atomic_epoch_and_ts*atomic_epoch_and_ts){
	return load_relaxed(atomic_epoch_and_ts);
}

void atomic_store_epoch_and_ts(atomic_epoch_and_ts*pepoch_and_ts,atomic_epoch_and_ts new_epoch_and_ts){
	return store_relaxed(pepoch_and_ts,new_epoch_and_ts);
}

void set_epoch(atomic_epoch_and_ts *atomic_epoch_and_ts,unsigned int epoch){
	unsigned int*pepoch=(unsigned int*)&(atomic_epoch_and_ts->high);
	*pepoch=epoch;
	return;
}

void set_timestamp(atomic_epoch_and_ts *atomic_epoch_and_ts,double ts){
	double*pts=(double*)&(atomic_epoch_and_ts->low);
	*pts=ts;
	return;
}
unsigned int get_epoch(atomic_epoch_and_ts atomic_epoch_and_ts){
	unsigned int*pepoch=(unsigned int*)&(atomic_epoch_and_ts.high);
	return *pepoch;
}
double get_timestamp(atomic_epoch_and_ts atomic_epoch_and_ts){
	double*pts=(double*)&(atomic_epoch_and_ts.low);
	return *pts;
}