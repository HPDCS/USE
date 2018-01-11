#ifndef _PRINTS_H_
#define _PRINTS_H_

#if VERBOSE != 0
#define printlp(format, ...) printf("T%lu LP%lu :: " format, tid, current_lp, ##__VA_ARGS__);
#define printth(format, ...) printf("T%lu :: " format, tid, ##__VA_ARGS__);
#else
#define printlp(...)
#define printth(...)
#endif

#endif