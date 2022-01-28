#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_

#include <events.h>
#include <stdbool.h>

//array of structures -- each field represents the state of the lp being locked
typedef struct lp_local_struct {

	unsigned int lp;
	double hotness;
	simtime_t distance_curr_evt_from_gvt; //"delta_C ??"
	simtime_t distance_last_ooo_from_gvt; //"delta_R ??"
   bool evicted; //flag to signal eviction for the field
   //local-queue ptr for each lp -- to search the next event to be processed

} lp_local_struct;


/*
circular-buffer-like structure -- attributes relative to the local array state

typedef struct lp_local_struct {

   LP_state *lp_array; //array of lp used -> hotness, delta_C and delta_R are lp's attributes 
   unsigned int last_inserted_idx; //index in lp_array of the last inserted element
   unsigned int last_evicted_idx; //index in lp_array of the last evicted element
   unsigned int elements; //current number of elements being inserted
   unsignet int capacity; //total capacity
   

} lp_local_struct;


*/




/* init the structure */
void init_struct(void);

/* clean the structure */
void clean_struct(void);


/* ------ Metrics computing functions ------ */

/* function to compute "delta_C" and "delta_R" */
void compute_distance_from_gvt(lp_local_struct *lp_local); //,event.timestamp, last_ooo.timestamp);

/* function to compute hotness */
void compute_hotness(lp_local_struct *lp_local); //, lp.delta_C, lp.delta_R);



/*------ Array management functions ------ */

/* lp_local_struct field creation and lp insertion into local array
   hotness is 1 when the lp is locked
   distance_curr_evt_from_gvt and distance_last_ooo_from_gvt must be computed later */
void insert_lp(unsigned int new_lp); //LP_state *new_lp??);


/* choose the lp to be evicted among the warm ones
   use hotness function to determine which one must go -> min hotness
   return the index of the lp (or the position in lp_array) to be evicted */
unsigned int detect_lp_to_be_evicted(void);


/* lp eviction from local array
   call detect_lp_to_be_evicted to find the lp to evict
   then do the eviction */
void evict_lp(void);


/* choose the best candidate to be executed next
   first look at the events for current lp, otherwise use hotness function to determine the best lp
   if there is an empty spot in the array choose from global queue
   return possibly a different lp_idx */
unsigned int schedule_next_event(unsigned int curr_lp); //, LP_state *curr_lp);



#endif