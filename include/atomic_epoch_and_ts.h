#pragma once
#ifndef ATOMIC_EPOCH_TS_H
#define ATOMIC_EPOCH_TS_H
#include <atomic_16byte.h>
typedef uint128_atomic atomic_epoch_and_ts;

atomic_epoch_and_ts atomic_load_epoch_and_ts(atomic_epoch_and_ts*atomic_epoch_and_ts);
void atomic_store_epoch_and_ts(atomic_epoch_and_ts*pepoch_and_ts,atomic_epoch_and_ts new_epoch_and_ts);
void set_epoch(atomic_epoch_and_ts *atomic_epoch_and_ts,unsigned int epoch);
void set_timestamp(atomic_epoch_and_ts *atomic_epoch_and_ts,double ts);
unsigned int get_epoch(atomic_epoch_and_ts atomic_epoch_and_ts);
double get_timestamp(atomic_epoch_and_ts atomic_epoch_and_ts);

#endif
