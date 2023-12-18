#ifndef _PRINTS_H_
#define _PRINTS_H_


#ifndef NDEBUG
#define LOG(STRING,  ...)     (printf( (STRING), __VA_ARGS__))
#else
#define LOG(STRING,  ...) do{}while(0)
#endif


#if PRINT_SCREEN == 1
	#define COLOR_RED   	"\x1b[31m"
	#define RED(s)			"\x1b[31m"s"\x1b[0m"
	#define COLOR_GREEN   	"\x1b[32m"
	#define GREEN(s)  		"\x1b[32m"s"\x1b[0m"
	#define COLOR_YELLOW  	"\x1b[33m"
	#define YELLOW(s)  		"\x1b[33m"s"\x1b[0m"
	#define COLOR_BLUE    	"\x1b[34m"
	#define BLUE(s)  		"\x1b[34m"s"\x1b[0m"
	#define COLOR_MAGENTA 	"\x1b[35m"
	#define MAGENTA(s) 		"\x1b[35m"s"\x1b[0m"
	#define COLOR_CYAN    	"\x1b[36m"
	#define CYAN(s)  		"\x1b[36m"s"\x1b[0m"
	#define COLOR_RESET   	"\x1b[0m"
	
	#define BOLDBLACK(s)   "\033[1m\033[30m"s"\x1b[0m"      /* Bold Black */
	#define BOLDRED(s)     "\033[1m\033[31m"s"\x1b[0m"      /* Bold Red */
	#define BOLDGREEN(s)   "\033[1m\033[32m"s"\x1b[0m"      /* Bold Green */
	#define BOLDYELLOW(s)  "\033[1m\033[33m"s"\x1b[0m"      /* Bold Yellow */
	#define BOLDBLUE(s)    "\033[1m\033[34m"s"\x1b[0m"      /* Bold Blue */
	#define BOLDMAGENTA(s) "\033[1m\033[35m"s"\x1b[0m"      /* Bold Magenta */
	#define BOLDCYAN(s)    "\033[1m\033[36m"s"\x1b[0m"      /* Bold Cyan */
	#define BOLDWHITE(s)   "\033[1m\033[37m"s"\x1b[0m"      /* Bold White */
#else
	#define COLOR_RED   	""
	#define RED(s)			s
	#define COLOR_GREEN   	""
	#define GREEN(s)  		s
	#define COLOR_YELLOW  	""
	#define YELLOW(s)  		s
	#define COLOR_BLUE    	""
	#define BLUE(s)  		s
	#define COLOR_MAGENTA 	""
	#define MAGENTA(s) 		s
	#define COLOR_CYAN    	""
	#define CYAN(s)  		s
	#define COLOR_RESET   	""
	
	#define BOLDBLACK(s)   s      /* Bold Black */
	#define BOLDRED(s)     s      /* Bold Red */
	#define BOLDGREEN(s)   s      /* Bold Green */
	#define BOLDYELLOW(s)  s      /* Bold Yellow */
	#define BOLDBLUE(s)    s      /* Bold Blue */
	#define BOLDMAGENTA(s) s      /* Bold Magenta */
	#define BOLDCYAN(s)    s      /* Bold Cyan */
	#define BOLDWHITE(s)   s      /* Bold White */
#endif

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
