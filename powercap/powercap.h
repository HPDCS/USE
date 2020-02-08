/////////////////////////////////////////////////////////////////
//	Macro definitions
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//	Includes 
/////////////////////////////////////////////////////////////////

#include "stats_t.h"
#include  <stdio.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>

extern int* running_array;				// Array of integers that defines if a thread should be running
extern double net_error_accumulator;
extern long net_time_sum;
extern long net_energy_sum;


/////////////////////////////////////////////////////////////////
//	Function declerations
/////////////////////////////////////////////////////////////////


static inline void check_running_array(int threadId){
    while(running_array[threadId] == 0){
        pause();
    }
}
extern void init_powercap_mainthread(unsigned int threads);
extern void end_powercap_mainthread();
extern void init_powercap_thread(unsigned int id);
extern void sample_average_powercap_violation();
extern void start_heuristic(double throughput, bool good_saple);
//extern void reset_euristic();