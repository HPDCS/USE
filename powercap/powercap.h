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

extern int* running_array;				// Array of integers that defines if a thread should be running


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
extern void start_euristic(double throughput);
extern void reset_euristic();