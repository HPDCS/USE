#ifndef _PRINTS_H_
#define _PRINTS_H_

#if DEBUG == 1
#define printlp(format, ...) printf("T%lu LP%lu :: " format, tid, current_lp, ##__VA_ARGS__);
#else
#define printlp(...)
#endif

#endif