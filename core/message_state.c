#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include <lookahead.h>
#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;
static int *current_region;

extern int queue_lock;

/*MAURO, DA ELIMINARE*/
simtime_t get_processing(unsigned int i){
	return current_time_vector[i];
}


void message_state_init(void){
    unsigned int i;

    current_time_vector = malloc(sizeof(simtime_t) * n_cores);
    current_region = malloc(sizeof(int)*n_cores);
    
    if(current_time_vector == NULL){
		printf("Out of memory in %s:%d", __FILE__, __LINE__);
		abort();		
	}
    for(i = 0; i < n_cores; i++){
        current_time_vector[i] = INFTY;     //processing
		current_region[i]=0;
    }
}

void execution_time(simtime_t time, int clp){
    current_time_vector[tid] = time;      //processing
    current_region[tid] = clp;
}

unsigned int check_safety_no_lookahead(simtime_t time){
    unsigned int i;
    unsigned int events;
    
    events = 0;
    
    for(i = 0; i < n_cores; i++){
        if(i!=tid && (time > current_time_vector[i] || (time==current_time_vector[i] && tid > i)))
            events++;
    }
    
    return events;
}

unsigned int check_safety_lookahead(simtime_t time){
    unsigned int i;
    unsigned int events;
    
    time -= LOOKAHEAD;//invece di sommarlo ogni volta al tempo che sto studiando, lo sottraggo a time una volta sola
    events = 0;
    
    for(i = 0; i < n_cores; i++){
        if(i!=tid && (time > current_time_vector[i] || (time==current_time_vector[i] && tid > i)))
            events++;
    }
    
    return events;
}

unsigned int check_safety(simtime_t time){
    unsigned int i;
    unsigned int events;
    events = 0;
    
    for(i = 0; i < n_cores; i++){
		
        if(i!=tid && (
			(   (time > (current_time_vector[i]+LOOKAHEAD)) || (time==(current_time_vector[i]+LOOKAHEAD) && tid > i))
			||
			( (current_lp==current_region[i]) && (time > current_time_vector[i] || (time==current_time_vector[i] && tid > i) ) )
		  ))
            events++;
    }
    return events;
}

void flush(void) {
    while(__sync_lock_test_and_set(&queue_lock, 1))
        while(queue_lock);

	queue_deliver_msgs();

    __sync_lock_release(&queue_lock);
}



