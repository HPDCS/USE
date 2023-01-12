#ifndef _PRINTS_H_
#define _PRINTS_H_


#include <hpdcs_utils.h>
#include <queue.h>

#if DEBUG == 1

#define printlp(format, ...) printf("T%u LP%u :: " format, tid, current_lp, ##__VA_ARGS__);
#define printth(format, ...) printf("T%u :: " format, tid, ##__VA_ARGS__);

#define print_event(event)	printf("[LP:%u->%u]: TS:%f TB:%u EP:%u STATE:%#02x(%s) IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",\
							event->sender_id,\
							event->receiver_id,\
							event->timestamp,\
							event->tie_breaker,\
							event->epoch,\
							event->state,\
							evt_state_str(event->state),\
							is_valid(event),\
							event,\
							event->node)

#else

#define printlp(...)
#define printth(...)
#define print_event(event)

#endif // DEBUG == 1

#define evt_state_str(state) state == 1 ? "EXTRACTED" : state == 2 ? "ELIMINATED" : state == 3 ? "ANTI_EVENT" : "UNKNOWN"


#endif
