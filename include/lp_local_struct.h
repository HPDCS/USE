#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_

#include <events.h>
#include <core.h>
#include <queue.h>
#include <local_index/local_index.h>
#include <stdbool.h>
#include <simtypes.h>
#include <prints.h>
#include <hpipe.h>

#ifndef CURRENT_BINDING_SIZE
#define CURRENT_BINDING_SIZE       1
#endif

#ifndef EVICTED_BINDING_SIZE
#define EVICTED_BINDING_SIZE       1
#endif

#ifndef START_WINDOW
#define START_WINDOW		   0.4
#endif

__thread simtime_t MAX_LOCAL_DISTANCE_FROM_GVT = START_WINDOW;


// TODO revise implementation of pipe. Specification is: LIFO SET - DONE
// TODO implentation of dynamic MAX_LOCAL_DISTANCE_FROM_GVT
// TODO hierarchical arragement of evicted pipe
// TODO Implement a global fetch that might get already locked events -- DONE


//array of structures -- each field represents the state of the lp being locked
typedef struct pipe_entry {
	double hotness;
	simtime_t distance_curr_evt_from_gvt; 
	simtime_t distance_last_ooo_from_gvt; 
    simtime_t next_ts;
    unsigned int lp;
} pipe_entry_t;

typedef struct pipe{
  //pipe_entry_t entries[CURRENT_BINDING_SIZE];
  size_t next_to_insert;   //next index of the array where to insert an lp
  size_t size;
  pipe_entry_t entries[];
} pipe_t;

extern __thread pipe_t *thread_locked_binding;
extern __thread pipe_t *thread_unlocked_binding;
extern  __thread int local_schedule_count;
extern pipe_t *evicted_pipe_pointers[HPIPE_INDEX1_LEN];


/* init the structure */
static pipe_t* init_struct(size_t size){
    size_t i;
    pipe_t *pipe = malloc(sizeof(pipe_t)+size*sizeof(pipe_entry_t));
    pipe->size           =  size;
    pipe->next_to_insert =  0;

    for(i = 0; i < pipe->size; i++){
        pipe->entries[i].lp = UNDEFINED_LP;
        pipe->entries[i].hotness = 0;
        pipe->entries[i].distance_curr_evt_from_gvt = 0;
        pipe->entries[i].distance_last_ooo_from_gvt = 0;
        pipe->entries[i].next_ts = INFTY;
    }
    return pipe;

}

/* clean the structure */
void clean_struct(void);

/* ------ Metrics computing functions ------ */

/* function to compute "delta_C" and "delta_R" */
void compute_distance_from_gvt(pipe_t *lp_local); //,event.timestamp, last_ooo.timestamp);

/* function to compute hotness */
void compute_hotness(pipe_t *lp_local); //, lp.delta_C, lp.delta_R);


static inline void update_pipe_entry(pipe_t *pipe, unsigned int index, unsigned int lp, simtime_t next_ts){
  #if DEBUG==1
    assertf(index >= pipe->size, "out-of-boundary insertion in pipe %lu %u\n", pipe->size, index);
  #endif
    pipe->entries[index].lp = lp;
    pipe->entries[index].next_ts = INFTY;
    pipe->entries[index].distance_curr_evt_from_gvt = INFTY;
    if(next_ts != INFTY){
        pipe->entries[index].next_ts = next_ts;
        pipe->entries[index].distance_curr_evt_from_gvt = next_ts - local_gvt;
    }
}


/* The function inserts an lp into a pipe, might be the evicted pipe too
@param: pipe a pointer to the to-be-updated pipe
@param: new_lp The new lp to insert
@param: size The size of the pipe
@param last_inserted The index of the most recent element inserted into the pipe
@param next_to_insert The index of the next element to insert
It return the index of an element to be evicted, or UNDEFINED_LP if no eviction must take place
*/
static inline unsigned int insert_lp_in_pipe(pipe_t *pipe, unsigned int new_lp, simtime_t next_ts, pipe_entry_t *entry_to_be_evicted) {

    unsigned int next_to_insert = pipe->next_to_insert;
  #if VERBOSE == 1
    unsigned int last_inserted = pipe->next_to_insert == 0 ? (pipe->size-1) : (pipe->next_to_insert-1);
  #endif
    unsigned int oldest = (pipe->next_to_insert+1) % pipe->size;
    size_t idx = 0,  i = 0;


    //if the lp is in pipe it becomes a standing element

    // look for an entry already associated to the lp
    for(i = 0; i<pipe->size; i++){
        idx = (oldest+i) % pipe->size;
        if(pipe->entries[idx].lp == new_lp) break; 
    }
    /*
    for(;      i<pipe->size-1; i++){
        prev_idx = (oldest+i)   % pipe->size;
             idx = (oldest+i+1) % pipe->size;
        pipe[prev_idx] = pipe[idx];
    }*/
    next_to_insert = pipe->next_to_insert = idx;

  #if DEBUG == 1
    assertf(idx != next_to_insert, "idx != next_to_insert %lu %u\n", idx, next_to_insert); 
  #endif
    *entry_to_be_evicted = pipe->entries[next_to_insert];
    update_pipe_entry(pipe, next_to_insert, new_lp, next_ts);
    pipe->next_to_insert = (next_to_insert+1) % pipe->size;;

  #if VERBOSE == 1
    printf("OLDEST: %d - \t LAST_INSERTED: %d - \t NEXT_TO_INSERT: %d \n", oldest, last_inserted, next_to_insert);
  #endif

    return entry_to_be_evicted->lp;
}


/* The function finds the next event from one lp 
@param : index The index of the current pipe field to check
@param : cur_lp The index of the lp being processed
@param : current_lp_distance The distance of next event from local gvt to update
It returns the event to be analyzed
*/
static inline msg_t * find_next_event_from_lp(unsigned cur_lp) {

    assertf(cur_lp == UNDEFINED_LP, "trying to find_next_event_from_lp from UNDEFINED_LP%s", "\n");
    // flush input channel
    process_input_channel(LPS[cur_lp]);

    // get the next event and compute its distance from current_lvt
    msg_t *next_evt = NULL;
    if(LPS[cur_lp]->actual_index_top != NULL)  next_evt = LPS[cur_lp]->actual_index_top->payload;

    return next_evt;

}


/* The function detects the best lp to process next prioritizing the last inserted one
@param: local_pipe The pipe
@param: min_lp The designed lp to be chosen
@param: minimum_diff The minimum interval between next event and local gvt
@param min_local_evt The next event to schedule of the best lp chosen
*/
static inline unsigned int detect_best_event_to_schedule(pipe_t *pipe) {
  #if DEBUG == 1
    assertf(!pipe, "trying to schedule from a %p pipe\n", pipe);
  #endif
    size_t i,idx;
    size_t next_to_insert = pipe->next_to_insert; 
    size_t last_inserted = next_to_insert == 0 ? (pipe->size-1) : (next_to_insert-1);
    unsigned int min_lp = UNDEFINED_LP;
  
    idx = last_inserted;
    for(i=0;i<pipe->size;i++){
        unsigned int lp = pipe->entries[idx].lp; 
      #if DEBUG == 1
        assertf(pipe == &thread_locked_binding && lp != UNDEFINED_LP && !haveLock(lp), "found %u in the locked pipe without lock\n", lp);
      #endif
        if(lp == UNDEFINED_LP) continue;
        if(pipe->entries[idx].distance_curr_evt_from_gvt < MAX_LOCAL_DISTANCE_FROM_GVT){
            min_lp = lp;
            break;
        }

        idx = idx == 0 ? (pipe->size-1) : (idx-1);
    }
    return min_lp;
}

//#undef CURRENT_BINDING_SIZE




#endif
