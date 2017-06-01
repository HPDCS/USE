#ifndef __UTIL_HPCDS__
#define __UTIL_HPCDS__

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

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
        exit(1);\
        }
#else
#define assertf(CONDITION, STRING,  ...) {}
#endif

#ifndef NDEBUG
#define LOG(STRING,  ...)     (printf( (STRING), __VA_ARGS__))
#else
#define LOG(STRING,  ...) do{}while(0)
#endif

#endif
