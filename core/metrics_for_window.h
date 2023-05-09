#ifndef __METRICS_FOR_WINDOW_H
#define __METRICS_FOR_WINDOW_H

#include <window.h>


extern window w;
extern void init_metrics_for_window();
extern void aggregate_metrics_for_window_management(window *w);
extern int check_window();
extern simtime_t get_current_window();

#endif