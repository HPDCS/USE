#include <stdlib.h>
#include <limits.h>

#include <queue.h>
#include "core.h"
#include "message_state.h"



static simtime_t *current_time_vector;

static simtime_t *outgoing_time_vector;

static simtime_t gvt; //da cancellare



extern int queue_lock;

/*MAURO, DA ELIMINARE*/
simtime_t get_processing(unsigned int i){return current_time_vector[i];}
simtime_t get_intransit(unsigned int i){return outgoing_time_vector[i];}


void message_state_init(void){
    int i;

    current_time_vector = malloc(sizeof(simtime_t) * n_cores);
    outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);

    for(i = 0; i < n_cores; i++){
        current_time_vector[i] = INFTY;     //processing
        outgoing_time_vector[i] = INFTY;    //in_transit
    }
}

void execution_time(simtime_t time){
    current_time_vector[tid] = time;      //processing
    outgoing_time_vector[tid] = INFTY;    //in transit
}
/* autore: Mauro Ianni */
//da rivedere se cosi va bene rispetto all'originale
unsigned int check_safety(){
    unsigned int i, k;
    unsigned int events = 0;
    simtime_t ctv, gvt_temp;
/*
    check_safety_old(&events);
    return events;
*/


    while(__sync_lock_test_and_set(&queue_lock, 1)) while(queue_lock);

    for(i = 0; i < n_cores; i++){
        if(i!=tid){
            //while(__sync_lock_test_and_set(&queue_lock, 1))
            //    while(queue_lock);

            ctv=current_time_vector[i];
            //otv=outgoing_time_vector[i];	///l'outgoing_time_vector può essere tolto

            //__sync_lock_release(&queue_lock);

            //min = (ctv<otv ? ctv : otv);

            if( current_lvt > ctv || (current_lvt==ctv && tid > i) )
                events++;
        }
    }

    if(events==0){ ///da cancellare: mi serve solo per dei controlli qui
		
		//printf("commit: %fn",current_lvt);

        if(current_lvt>105 && current_lvt<106){
            for(k=0; k < n_cores; k++){
                        printf("G: porcessing[%d] =%e, in_transit[%d]=%e\n", k,get_processing(k),k, get_intransit(k));
                    }
            fflush(stdout);
        }
        if(gvt>current_lvt){
            printf("NEW: il gvt è %f, il thread %u al tempo %f lo sta violando\n", gvt, tid, current_lvt);
            for(k=0; k < n_cores; k++){
                        printf("GV: processing[%d] =%e, in_transit[%d]=%e\n", k,get_processing(k),k, get_intransit(k));
                    }
            printf("-----------------------------------------------------------------\n");
            abort();
        }
        //gvt = current_lvt;
        do{ //aggiornamento del gvt, se non serve si può togliere
			gvt_temp = gvt;	
			if(current_lvt<gvt_temp)
				break; 
		}while(double_cas(&gvt, gvt_temp, current_lvt)==gvt_temp);
		
    } //da cancellare

    __sync_lock_release(&queue_lock);

    return events;
}

/*
int check_safety_old(unsigned int *events){
  int i;
  unsigned int min_tid = n_cores + 1;
  double min = INFTY;
  int ret = 0;

  events[0] = 0;

  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);

  for(i = 0; i < n_cores; i++)
  {

    if( (i != tid) && ((current_time_vector[i] < min) || (outgoing_time_vector[i] < min)) )
    {
      min = ( current_time_vector[i] < outgoing_time_vector[i] ?  current_time_vector[i] : outgoing_time_vector[i]  );
      min_tid = i;
      events[0]++;
    }
  }

  if(current_time_vector[tid] < min) {
	ret = 1;
	if(gvt>current_time_vector[tid]){
            printf("il gvt è %f il thread %u al tempo %f lo sta violando\n", gvt, tid, current_time_vector[tid]);
            printf("-----------------------------------------------------------------\n");
            //abort();
        }
	gvt=current_time_vector[tid];
	goto out;
  }

  if(current_time_vector[tid] == min && tid < min_tid) {
    ret = 1;
    if(gvt>current_time_vector[tid]){
            printf("il gvt è %f il thread %u al tempo %f lo sta violando\n", gvt, tid, current_time_vector[tid]);
            printf("-----------------------------------------------------------------\n");
            //abort();
        }
    gvt=current_time_vector[tid];
  }


 out:
  __sync_lock_release(&queue_lock);

  return ret;
}
*/

void flush(void) {
    double t_min;
    while(__sync_lock_test_and_set(&queue_lock, 1))
        while(queue_lock);
        
	t_min = queue_deliver_msgs();

	///current_time_vector[tid] += lookahead; //da implementare il lookahead
	outgoing_time_vector[tid] = t_min;


    __sync_lock_release(&queue_lock);
}



