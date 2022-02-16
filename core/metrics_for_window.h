#ifndef __METRICS_FOR_WINDOW_H
#define __METRICS_FOR_WINDOW_H

#include <pthread.h>
#include <window.h>


extern window w;
extern simtime_t *window_size;
extern int *enabled;

extern pthread_mutex_t stat_mutex, window_check_mtx;

extern unsigned int prev_comm_evts, prev_comm_evts_ref, prev_first_time_exec_evts, prev_comm_evts_window; 
extern __thread simtime_t granularity_ref;
extern simtime_t prev_granularity;
extern __thread double old_thr;

extern __thread clock_timer start_window_reset;
extern __thread clock_timer start_metrics_interval;
extern __thread stat64_t time_interval_for_metrics_comput, time_interval_for_window_reset;



extern void init_metrics_for_window();
extern void aggregate_metrics_for_window_management();

#endif