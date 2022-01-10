#ifndef __LP_LOCAL_STRUCT_H_
#define __LP_LOCAL_STRUCT_H_


#define SIZE //size


typedef struct lp_local_struct {

	unsigned int lp;
	double hotness;
	simtime_t distance_curr_evt_from_gvt; //"delta_C ??"
	simtime_t distance_last_ooo_from_gvt; //"delta_R ??"

} lp_local_struct;

lp_local_struct local_array[SIZE];

/* init the structure */
void init_struct(void);


/* ------ Metrics computing functions ------ */

/* function to compute "delta_C" and "delta_R" */
void compute_distance_from_gvt(lp_local_struct *lp_local); //,event.timestamp, //last_ooo.timestamp);

/* function to compute hotness */
void compute_hotness(lp_local_struct *lp_local);



/*------ Array management functions ------ */

/* lp_local_struct field creation and lp insertion into local array
   hotness is 1 when the lp is locked
   distance_curr_evt_from_gvt and distance_last_ooo_from_gvt must be computed later */
void insert_lp(unsigned int new_lp, double hotness);

/* choose the lp to be evicted among the warm ones
   use hotness function to determine which one must go
   return the index of the lp (or the position in the array?) to be evicted */
unsigned int detect_lp_to_be_evicted(void);

/* lp eviction from local array */
void evict_lp(unsigned int lp_idx);

/* choose the best candidate to be executed next
   first look at the events for current lp, otherwise use hotness function to determine the best lp
   if there is an empty spot in the array choose from global queue
   return possibly a different lp_idx */
unsigned int schedule_next_event(unsigned int curr_lp);



#endif