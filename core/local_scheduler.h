#ifndef __LOCAL_SCHEDULER_H__
#define __LOCAL_SCHEDULER_H__

#include <pthread.h>

extern void local_binding_init();
extern void local_binding_push(int lp);
extern int local_fetch();
extern void flush_locked_pipe();
extern pthread_barrier_t local_schedule_init_barrier;
#endif