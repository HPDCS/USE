#ifndef _PRINTS_H_
#define _PRINTS_H_

#if VERBOSE != 0
#define printlp(format, ...) printf("T%lu LP%lu :: " format, tid, current_lp, ##__VA_ARGS__);
#define printth(format, ...) printf("T%lu :: " format, tid, ##__VA_ARGS__);
#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event));
#else
#define printlp(...)
#define printth(...)
#endif

#endif
