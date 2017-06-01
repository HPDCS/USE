#ifndef _STATISTICS_H_
#define _STATISTICS_H_


#define EVENTS_SAFE 0
#define EVENTS_HTM 1 //DEL
#define EVENTS_STM 2
#define EVENTS_ROLL 30

#define COMMITS_HTM 3 //DEL
#define COMMITS_STM 4

#define CLOCK_SAFE 5
#define CLOCK_HTM 6 //DEL
#define CLOCK_STM 7

#define CLOCK_HTM_THROTTLE 8 //DEL
#define CLOCK_STM_WAIT 9
#define CLOCK_UNDO_EVENT 10
#define CLOCK_ENQUEUE 22
#define CLOCK_LOOP 24
#define CLOCK_DEQUEUE 23
#define CLOCK_DEQ_LP 31
#define CLOCK_DELETE 32

#define ABORT_UNSAFE 11		//DEL
#define ABORT_REVERSE 12    //DEL
#define ABORT_CONFLICT 13   //DEL
#define ABORT_CACHEFULL 14  //DEL
#define ABORT_DEBUG 15      //DEL
#define ABORT_NESTED 16     //DEL
#define ABORT_GENERIC 17    //DEL
#define ABORT_RETRY 18      //DEL
#define ABORT_TOTAL 19      //DEL

#define EVENTS_FETCHED 20
#define EVENTS_FLUSHED 33
#define T_BTW_EVT 21
#define PRIO_THREADS 22
#define PENDING_EVENTS 23
#define TOTAL_ATTEMPTS 24

struct stats_t {
    unsigned int events_total;
    unsigned int events_safe;
    unsigned int events_htm;//
    unsigned int events_stm;
    unsigned int events_roll;
    
    unsigned int commits_htm;//
    unsigned int commits_stm;
    unsigned int commits_stm_tmp;

    unsigned long long clock_safe;
    unsigned long long clock_htm;//
    unsigned long long clock_stm;
    unsigned long long clock_htm_throttle;//
    unsigned long long clock_stm_wait;
    unsigned long long clock_undo_event;
    unsigned long long clock_enqueue;
    unsigned long long clock_dequeue;
    unsigned long long clock_deq_lp;
    unsigned long long clock_loop;
    unsigned long long clock_delete;

    unsigned int abort_total;//
    unsigned int abort_unsafe;//
    unsigned int abort_reverse;//
    unsigned int abort_conflict;//
    unsigned int abort_cachefull;//
    unsigned int abort_debug;//
    unsigned int abort_nested;//
    unsigned int abort_generic;//
    unsigned int abort_retry;//
    
	unsigned long long events_fetched;
	unsigned long long events_flushed;
} __attribute__((aligned (64)));


extern struct stats_t *thread_stats;


void statistics_init();

void statistics_fini();

unsigned long long get_time_of_an_event();

unsigned long long get_useful_time_htm();

unsigned long long get_useful_time_stm();

unsigned long long get_useful_time_htm_th(unsigned int t);

unsigned long long get_useful_time_stm_th(unsigned int t);

double get_frac_htm_aborted();

void print_statistics();

void statistics_post_data(int lid, int type, double value);

#endif // _STATISTICS_H_
