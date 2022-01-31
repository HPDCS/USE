#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_

#include <events.h>
#include <core.h>
#include <stdbool.h>

//array of structures -- each field represents the state of the lp being locked
//note: avere un unico array di size pipe+evicted_pipe per locked ed evicted, indice che segna 
//l'inizio della zona per gli evicted, e poi i due indici last_inserted e next_to_insert sia per
//zona locked che zona evicted
typedef struct pipe {

	int lp;
	double hotness;
	simtime_t distance_curr_evt_from_gvt; 
	simtime_t distance_last_ooo_from_gvt; 
   //bool evicted; //flag to signal eviction for the field


} pipe;






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
static inline int insert_lp_in_pipe(int *lp, int new_lp, size_t size, int *last_inserted, int *next_to_insert) {

   int lp_to_evict = UNDEFINED_LP;

   if (*last_inserted == *next_to_insert) { //empty pipe 
      *lp = new_lp;
      *next_to_insert = (*next_to_insert + 1) % size;
      return -1;
   } 
   

   if (*lp != -1) { lp_to_evict = *lp; } //lp to be evicted
        

   *lp = new_lp; 
   

   *last_inserted = *next_to_insert;
   *next_to_insert = (*next_to_insert + 1) % size;
   

   return lp_to_evict;
}


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