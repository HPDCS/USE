#ifndef _STATISTICS_H_
#define _STATISTICS_H_


#define EVENTS_SAFE 0
#define EVENTS_HTM 1
#define EVENTS_STM 2
#define COMMITS_HTM 3
#define COMMITS_STM 4
#define CLOCK_SAFE 5
#define CLOCK_HTM 6
#define CLOCK_STM 7
#define CLOCK_HTM_THROTTLE 8
#define CLOCK_STM_WAIT 9
#define CLOCK_UNDO_EVENT 10
#define ABORT_UNSAFE 11
#define ABORT_REVERSE 12
#define ABORT_CONFLICT 13
#define ABORT_CACHEFULL 14
#define ABORT_DEBUG 15
#define ABORT_NESTED 16
#define ABORT_GENERIC 17

struct stats_t {
    unsigned int events_total;
    unsigned int events_safe;
    unsigned int events_htm;
    unsigned int events_stm;
    
    unsigned int commits_htm;
    unsigned int commits_stm;

    double clock_safe;
    double clock_htm;
    double clock_stm;
    double clock_htm_throttle;
    double clock_stm_wait;

    unsigned int abort_unsafe;
    unsigned int abort_reverse;
    unsigned int abort_conflict;
    unsigned int abort_cachefull;
    unsigned int abort_debug;
    unsigned int abort_nested;
    unsigned int abort_generic;
};

extern struct stats_t *thread_stats;

void statistics_init();
void statistics_fini();

void statistics_post_data(int lid, int type, double value);

#endif // _STATISTICS_H_