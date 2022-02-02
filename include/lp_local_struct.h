#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_

#include <events.h>
#include <core.h>
#include <queue.h>
#include <local_index/local_index.h>
#include <stdbool.h>
#include <simtypes.h>
#include <prints.h>


#define CURRENT_BINDING_SIZE        3
#define MAX_LOCAL_DISTANCE_FROM_GVT 0.1


//array of structures -- each field represents the state of the lp being locked
typedef struct pipe {

	unsigned int lp;
	double hotness;
	simtime_t distance_curr_evt_from_gvt; 
	simtime_t distance_last_ooo_from_gvt; 
   //bool evicted; //flag to signal eviction for the field


} pipe;

__thread pipe thread_locked_binding[CURRENT_BINDING_SIZE];
__thread pipe thread_unlocked_binding[CURRENT_BINDING_SIZE];
__thread local_schedule_count = 0;

__thread int next_to_insert; //next index of the array where to insert an lp
__thread int last_inserted; //last index of the array where an lp has being inserted

__thread int next_to_insert_evicted;
__thread int last_inserted_evicted;


/* init the structure */
void init_struct(void);

/* clean the structure */
void clean_struct(void);


/* ------ Metrics computing functions ------ */

/* function to compute "delta_C" and "delta_R" */
void compute_distance_from_gvt(pipe *lp_local); //,event.timestamp, last_ooo.timestamp);

/* function to compute hotness */
void compute_hotness(pipe *lp_local); //, lp.delta_C, lp.delta_R);



/*------ Array management functions ------ */


/* The function checks if the lp to be evicted has more than one occurrence in pipe 
@param : pipe The local pipe
@param : lp The index of the lp candidate for the eviction
It returns true if the lp had more than one occurence in pipe, false otherwise
*/
static inline bool is_in_pipe(pipe pipe[], unsigned int lp) {

   int i, sum = 0;
   for (i = 0; i < CURRENT_BINDING_SIZE; i++) {
      if (pipe[i].lp == lp) sum++;
   }

   if (sum >= 1) {
      return true;
   } else {
      return false;
   }

}

/* The function inserts an lp into a pipe, might be the evicted pipe too
@param: lp The array of lp
@param: new_lp The new lp to insert
@param: size The size of the pipe
@param last_inserted The index of the most recent element inserted into the pipe
@param next_to_insert The index of the next element to insert
It return the index of an element to be evicted, or -1 if no eviction must take place


If lp_to_evict is >= 0 this function can be called to perform an eviction, where lp is the array
of the evicted pipe and new_lp is lp_to_evict
*/
static inline unsigned int insert_lp_in_pipe(unsigned int *lp, unsigned int new_lp, size_t size, int *last_inserted, int *next_to_insert) {

   unsigned int lp_to_evict = UNDEFINED_LP;


   if (*last_inserted == *next_to_insert) { //empty pipe 
      *lp = new_lp;
      *next_to_insert = (*next_to_insert + 1) % size;
      return UNDEFINED_LP;
   }
   

   if (*lp != UNDEFINED_LP) { lp_to_evict = *lp; } //lp to be evicted
        

   *lp = new_lp; 
   

   *last_inserted = *next_to_insert;
   *next_to_insert = (*next_to_insert + 1) % size;
   

   return lp_to_evict;
}


/* The function finds the next event from one lp 
@param : index The index of the current pipe field to check
@param : cur_lp The index of the lp being processed
@param : current_lp_distance The distance of next event from local gvt to update
It returns the event to be analyzed
*/
static inline msg_t * find_next_event_from_lp(unsigned *cur_lp, simtime_t *current_lp_distance, simtime_t *distance_curr_evt_from_gvt) {


    // flush input channel
    process_input_channel(LPS[*cur_lp]);

    // get the next event and compute its distance from current_lvt
    msg_t *next_evt = LPS[*cur_lp]->actual_index_top->payload;
    *current_lp_distance = next_evt->timestamp - local_gvt;

    *distance_curr_evt_from_gvt = *current_lp_distance;

    return next_evt;

}


/* The function detects the best lp to process next prioritizing the last inserted one
@param: local_pipe The pipe
@param: min_lp The designed lp to be chosen
@param: minimum_diff The minimum interval between next event and local gvt
@param min_local_evt The next event to schedule of the best lp chosen
*/
static inline void detect_best_event_to_schedule(pipe local_pipe[], unsigned int *min_lp, simtime_t *minimum_diff, msg_t **min_local_evt) {

    unsigned int best_lp, cur_lp;
    msg_t *last_inserted_evt, *next_evt;
    simtime_t last_inserted_distance, current_lp_distance;

    int i, j;

    //get last inserted lp as the best one to compare with others
    best_lp = local_pipe[last_inserted].lp;
    last_inserted_evt = find_next_event_from_lp(&best_lp, &last_inserted_distance, 
                &local_pipe[last_inserted].distance_curr_evt_from_gvt);

    if (last_inserted_distance < MAX_LOCAL_DISTANCE_FROM_GVT) {
            *min_lp = best_lp;
            *minimum_diff = local_pipe[last_inserted].distance_curr_evt_from_gvt;
            *min_local_evt = last_inserted_evt;
    }
    

    //iterate over pipe from the last inserted lps
    //check lp in insertion order
    i = last_inserted == 0 ? CURRENT_BINDING_SIZE-1 : last_inserted;
    j = 0;
    while (i+j != last_inserted) {

        //get lp
        cur_lp = local_pipe[i+j].lp;

        
        if (cur_lp == UNDEFINED_LP) {
            i--; 
            continue;
        }

        //find the next event for the current lp
        next_evt = find_next_event_from_lp(&cur_lp, &current_lp_distance, &local_pipe[i+j].distance_curr_evt_from_gvt);

        #if DEBUG == 1
            printlp("[%u]BEST_LP : %u ", tid, best_lp);printf("\t last_inserted_distance %f\n", last_inserted_distance);
            printlp("[%u]CUR_LP : %u ", tid, cur_lp);printf("\t current_lp_distance %f\n", current_lp_distance);
        #endif

        //if best_lp already satisfies the condition skip this and just flush events into local index of other lps
        if(*min_lp != best_lp && current_lp_distance < MAX_LOCAL_DISTANCE_FROM_GVT){ //&& current_lp_distance < minimum_diff){
            
            *min_lp       = local_pipe[i+j].lp;
            *minimum_diff = local_pipe[i+j].distance_curr_evt_from_gvt;
            *min_local_evt= next_evt;
            
        }

        if (i+j == 0) j = CURRENT_BINDING_SIZE-1;
        i--;
        
    }

    #if DEBUG == 1
            printlp("[%u] MIN_LP : %u\n", tid, *min_lp);
    #endif


}





#endif