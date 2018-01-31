#ifndef __UTIL_HPCDS__
#define __UTIL_HPCDS__

#include <signal.h>

#define gdb_abort __asm__("int $3")//raise(SIGINT)
	
#define NDEBUG 

#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 5,4,3,2,1)
#define VA_NUM_ARGS_IMPL(_1,_2,_3,_4,_5,N,...) N

#define macro_dispatcher(func, ...) macro_dispatcher_(func, VA_NUM_ARGS(__VA_ARGS__))
#define macro_dispatcher_(func, nargs) macro_dispatcher__(func, nargs)
#define macro_dispatcher__(func, nargs) func ## nargs

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define UNION_CAST(x, destType) (((union {__typeof__(x) a; destType b;})x).b)

#ifndef NDEBUG
#define assertf(CONDITION, STRING,  ...)        if(CONDITION)\
        {\
        printf( (STRING), __VA_ARGS__);\
		gdb_abort;\
        }
#else
#define assertf(CONDITION, STRING,  ...) {}
#endif

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


#endif
