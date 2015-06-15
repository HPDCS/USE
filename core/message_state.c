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


void message_state_init(void)
{
  int i;

  current_time_vector = malloc(sizeof(simtime_t) * n_cores);
  outgoing_time_vector = malloc(sizeof(simtime_t) * n_cores);

  for(i = 0; i < n_cores; i++)
  {
    current_time_vector[i] = INFTY;
    outgoing_time_vector[i] = INFTY; //TODO: Oppure a 0 ? riguardare...
  }
}

void execution_time(simtime_t time)
{
  current_time_vector[tid] = time;  //processing
  outgoing_time_vector[tid] = INFTY;//in transit

//  if(input_tid != tid && outgoing_time_vector[input_tid] == time)
//    outgoing_time_vector[input_tid] = INFTY;
}

void min_output_time(simtime_t time)
{
  outgoing_time_vector[tid] = time;
}

void commit_time(void)
{
  current_time_vector[tid] = INFTY;
}

/* autore: Mauro Ianni */
//da rivedere se cosi va bene rispetto all'originale
unsigned int check_safety(){
    unsigned int i;
    unsigned int events = 0;
    double curr_lvt, ctv, otv, min;

    curr_lvt=current_time_vector[tid]; //posso usare direttamente il current_lvt?

    for(i = 0; i < n_cores; i++){
        if(i!=tid){
            ctv=current_time_vector[i];
            otv=outgoing_time_vector[i];
            min = ((ctv<otv) ? ctv : otv);

            if( curr_lvt > min || (curr_lvt==min && tid > i) ){
                events++;
            }
        }
    }
    if(events==0){ //da cancellare
        if(gvt>curr_lvt){
            printf("il gvt è %f il thread %u al tempo %f lo sta violando\n", gvt, tid, current_time_vector[tid]);
            printf("-----------------------------------------------------------------\n");
            //abort();
        }
        gvt = curr_lvt;
    } //da cancellare

    return events;

  //Se events=0, vuol dire che non ci sono timestamp minori
  //Se events>0, vuol dire che ho eventi avanti a me, non sono safe e events mi fa da indicatore

}

int check_safety_old(unsigned int *events)
{
  int i;
  unsigned int min_tid = n_cores + 1;
  double min = INFTY;
  int ret = 0;

  events[0] = 0;

  //while(__sync_lock_test_and_set(&queue_lock, 1))
  //  while(queue_lock);

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
	/*if(gvt>current_time_vector[tid]){
            printf("il gvt è %f il thread %u al tempo %f lo sta violando\n", gvt, tid, current_time_vector[tid]);
            printf("-----------------------------------------------------------------\n");
            //abort();
        }*/
	gvt=current_time_vector[tid];
	goto out;
  }

  if(current_time_vector[tid] == min && tid < min_tid) {
    ret = 1;
    /*if(gvt>current_time_vector[tid]){
            printf("il gvt è %f il thread %u al tempo %f lo sta violando\n", gvt, tid, current_time_vector[tid]);
            printf("-----------------------------------------------------------------\n");
            //abort();
        }*/
    gvt=current_time_vector[tid];
  }


 out:
  //__sync_lock_release(&queue_lock);

  return ret;
}


void flush(void) {
  double t_min;
  while(__sync_lock_test_and_set(&queue_lock, 1))
    while(queue_lock);

  t_min = queue_deliver_msgs();

  if(t_min<current_time_vector[tid] && t_min!=0){//da cancellare
    printf("il thread %u at time %f ha generato un evento con tempo%f\n", tid, current_time_vector[tid],t_min);
    printf("-----------------------------------------------------------------\n");
    //abort();
  }
  outgoing_time_vector[tid] = t_min; //in_transit///ATTENZIONE: LI HO INVERTITI!!!!
  current_time_vector[tid] = INFTY;  //processing

  __sync_lock_release(&queue_lock);
}



