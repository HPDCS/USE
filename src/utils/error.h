#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdbool.h>

#define gdb_abort __asm__("int $3")//raise(SIGINT)

#ifndef NDEBUG
#define assertf(CONDITION, STRING,  ...)        if(CONDITION)\
        {\
        printf( (STRING), __VA_ARGS__);\
		gdb_abort;\
        }
#else
#define assertf(CONDITION, STRING,  ...) {}
#endif

extern void rootsim_error(bool fatal, const char *msg, ...);


#endif