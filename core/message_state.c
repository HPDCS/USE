#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;

static simtime_t gvt; //da cancellare

extern int queue_lock;

/*MAURO, DA ELIMINARE*/
simtime_t get_processing(unsigned int i){
	return current_time_vector[i];
}


void message_state_init(void){
    unsigned int i;

    gvt = 0;// da cancellare

    current_time_vector = malloc(sizeof(simtime_t) * n_cores);
    if(current_time_vector == NULL){
		printf("Out of memory in %s:%d", __FILE__, __LINE__);
		abort();		
	}
    for(i = 0; i < n_cores; i++){
        current_time_vector[i] = INFTY;     //processing
    }
}

void execution_time(simtime_t time){
    current_time_vector[tid] = time;      //processing
}

unsigned int check_safety(simtime_t time){
    unsigned int i;
    unsigned int events;
    
    events = 0;
    //while(__sync_lock_test_and_set(&queue_lock, 1)) while(queue_lock); // da cancellare

    for(i = 0; i < n_cores; i++){
        if(i!=tid && (time > current_time_vector[i] || (time==current_time_vector[i] && tid > i)))
            events++;
    }
/*
    if(events==0){ ///da cancellare: mi serve solo per dei controlli qui
		
		if(gvt>current_lvt){
            printf("GVT: il gvt e' %f, il thread %u al tempo %f lo sta violando\n", gvt, tid, current_lvt);
            for(i=0; i < n_cores; i++){
                        printf("GVT: processing[%d] =%e\n",get_processing(i),i);
                    }
            printf("GVT: -----------------------------------------------------------------\n");
            abort();
        }
        gvt=current_lvt;

    } //da cancellare

    __sync_lock_release(&queue_lock); // da cancellare

*/
    return events;
}

void flush(void) {
    while(__sync_lock_test_and_set(&queue_lock, 1))
        while(queue_lock);

	queue_deliver_msgs();

	///current_time_vector[tid] += lookahead; //da implementare il lookahead

    __sync_lock_release(&queue_lock);
}



