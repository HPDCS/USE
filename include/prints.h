#ifndef _PRINTS_H_
#define _PRINTS_H_

#if VERBOSE != 0
#define printlp(format, ...) printf("T%lu LP%lu :: " format, tid, current_lp, ##__VA_ARGS__);
#define printth(format, ...) printf("T%lu :: " format, tid, ##__VA_ARGS__);
#else
#define printlp(...)
#define printth(...)
#endif

#if DEBUG == 1
#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u STATE:%#02x(%s) IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n", event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, event->state, evt_state_str(event->state), is_valid(event), event, event->node)
#else
#define print_event(event)
#endif

#define evt_state_str(state) state == 1 ? "ESTRATTO" : state == 2 ? "CANCELLATO" : state == 3 ? "ANTI_EVENT" : "UNKNOWN"

#endif
