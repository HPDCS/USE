#ifndef __NUMA_MIGRATION_SUPPORT_H__
#define __NUMA_MIGRATION_SUPPORT_H__

#include <core.h>
#include <simtypes.h>
#include <timer.h>
#include <clock_constant.h>
#include <allocator.h>


#include <bitmap.h>
#include <window.h>
#include <local_scheduler.h>


extern void migrate_segment(unsigned int id, int numa_node);


#define BALANCE_OVERFLOW 0.30
#define UNBALANCE_ROUNDS_CHECK 10
#define SKEWNESS_THRESHOLD 0.6

/**
 * numa_struct represents the state of a numa node
 * 
 * numa_binding_bitmap: bitmap storing the LP binding to a numa node
 * unbalance_index: index of unbalance of a numa node used to plan a new rabalancing
 */
typedef struct numa_struct {
	unsigned numa_id;
	unsigned pad;
	simtime_t unbalance_index;			
	bitmap *numa_binding_bitmap;		
} numa_struct;


/**
 * lps_to_migrate represents the lps eligible for migration
 * 
 * lp_idx: index of the lp
 * expected_skewness: skewness of the destination numa node if the lp_idx would be migrated on it
 */
typedef struct lps_to_migrate {
    unsigned int lp_idx; 
    double expected_skewness;
    double migration_score;
} lps_to_migrate;


extern clock_timer numa_rebalance_timer;
extern unsigned int rounds_before_unbalance_check;
static volatile stat64_t time_interval_for_numa_rebalance;
static __thread double elapsed_time;


/**
 * This method allocates a new numa state
 * 
 * @param numa_state The numa state to be allocated
 */
static inline void alloc_numa_state(numa_struct *numa_state) {

	numa_state->numa_binding_bitmap = allocate_bitmap(n_prc_tot);
	numa_state->unbalance_index = 0.0;
}



/**
 * This method compares two values of the struct to order
 * 
 * @param left The left element to compare
 * @param right The right element to compare
 * 
 * @return -1 if left < right
 * 			1 if left > right
 * 			0 otherwise
 */
int compare(const void * left, const void * right) {
    const lps_to_migrate * left_lps = (const lps_to_migrate *) left;
    const lps_to_migrate * right_lps = (const lps_to_migrate *) right;

    if (left_lps->expected_skewness < right_lps->expected_skewness) {
        return -1;
    } else if (left_lps->expected_skewness > right_lps->expected_skewness) {
        return 1;
    } else {
        return 0;
    }
}


/**
 * This method calls qsort to sort the lps struct in ascending order
 * of expected skewness
 * 
 * @param lps_to_migrate_array The structure with eligible to migrate lps
 */
void sort_lp_to_migrate(lps_to_migrate * lps_to_migrate_array, unsigned int count) {

	qsort(lps_to_migrate_array, count, sizeof(lps_to_migrate), compare);

}



/**
 *This method sets the index of the LP to be migrated
 * and the relative expected skewness value
 * 
 * @param lps The lps to migrate
 * @param lp The index of the new LP eligible to be migrated
 * @param migration_score The migration score of the lp eligible to be migrated
 * @param max_node_unbalance_index The unbalance index of the max loaded numa node
 * @param min_node_unbalance_index The unbalance index of the min loaded numa node
 */
void set_lp_to_migrate(lps_to_migrate *lps, unsigned int lp, simtime_t migration_score, simtime_t max_node_unbalance_index, simtime_t min_node_unbalance_index) { 

	simtime_t max_diff = max_node_unbalance_index - migration_score;
	simtime_t min_diff = min_node_unbalance_index + migration_score;
	lps->lp_idx = lp;
	lps->expected_skewness = fabs(max_diff - min_diff);
	lps->migration_score   = migration_score;
}


/**
 *This method checks whether the periodic rebalance check for numa rebalancing
 * is needed or not by reading a timer
 * 
 * @param numa_timer The global timer
 * @return True if it is elapsed enough time, false otherwise
 */
static inline bool periodic_rebalance_check(clock_timer numa_timer) {

	time_interval_for_numa_rebalance = clock_timer_value(numa_timer);
	elapsed_time = time_interval_for_numa_rebalance / CLOCKS_PER_US;
	return (elapsed_time/1000.0 >= MEASUREMENT_PHASE_THRESHOLD_MS * UNBALANCE_ROUNDS_CHECK);

}

/**
 * This method sets the unbalance index of every numa node
 * as the sum of all the migration scores of the LPs belonging to the same numa node
 * 
 * @param numa_state The numa state of all nodes
 */
static inline void set_unbalance_index(numa_struct **numa_state) {

	/// computation of unbalance_index
	unsigned int i,j;
	int lp;
	for (i=0; i < num_numa_nodes; i++) { /// iterate numa nodes
		for (j=0; j < n_prc_tot; j++) { /// iterate n_proc_tot
			lp = get_bit(numa_state[i]->numa_binding_bitmap, j);
			if (lp) { /// if the corresponding bit is set in the lp bitmap compute the unbalance index
				assert(LPS[j]->ema_granularity > 0);
				assert(LPS[j]->ema_ti > 0);
				LPS[j]->migration_score = LPS[j]->ema_granularity/LPS[j]->ema_ti; // LPS[j]->ema_granularity;
				numa_state[i]->unbalance_index += LPS[j]->migration_score;
			}
		}
	}
}



/**
 * This method checks if a numa balance check must take place based on the number of rounds
 * executed managing the window.
 * 
 * @return True if the balance check must take place, false otherwise
 */
static inline bool enable_unbalance_check() {

	//TODO: define unbalance rounds check
	return (rounds_before_unbalance_check != 0 && rounds_before_unbalance_check % UNBALANCE_ROUNDS_CHECK == 0);

}



/**
 *This method checks whether the current numa node will be unbalanced or not
 * adding another LP to the balance threshold with a 30% tolerance over the unbalance index
 * @param numa_state The current numa state
 * @param lp The lp that might be bound to the node
 * @return True if the node is unbalanced, false otherwise
 */
bool is_node_unbalanced(numa_struct *numa_state, unsigned int lp){

#if VERBOSE == 1
	printf("is_node_unbalanced %f\n", numa_state->unbalance_index + LPS[lp]->migration_score);
#endif 

	return ( (numa_state->unbalance_index + LPS[lp]->migration_score) > (numa_state->unbalance_index + numa_state->unbalance_index*BALANCE_OVERFLOW) );
}



/**
 *This method chooses the max loaded numa node and the min loaded numa node among all the nodes
 * @param numa_state The numa state
 * @param max_load The max loaded numa node to set
 * @param min_load The min loaded numa node to set
 */
void choose_target_nodes(numa_struct **numa_state, numa_struct **max_load_numa_node, numa_struct **min_load_numa_node) { 

	simtime_t min_index, max_index;
	min_index = max_index = numa_state[0]->unbalance_index;
	*max_load_numa_node = *min_load_numa_node = numa_state[0];

	for (unsigned int node=1; node < num_numa_nodes; node++) {

		if (numa_state[node]->unbalance_index > max_index) {
			*max_load_numa_node = numa_state[node];
			max_index = numa_state[node]->unbalance_index;
		} else if (numa_state[node]->unbalance_index < min_index) {
			*min_load_numa_node = numa_state[node];
			min_index = numa_state[node]->unbalance_index;
		}

	}

}


/**
 *This method checks whether an lp is eligible for migration
 * @param lp The lp index
 * @param skewness The halved skewness between the max loaded node's and the min loaded node's unbalance index
 * @return True if the lp is eligible for migration, false otherwise
 */
bool eligible_for_migration(unsigned int lp, simtime_t skewness) {

#if VERBOSE == 1
	printf("eligible for migration %f\n", fabs(LPS[lp]->migration_score - skewness));
#endif


	// TODO: define skewness threshold
	return (fabs(LPS[lp]->migration_score - skewness) > skewness*SKEWNESS_THRESHOLD);
}



/**
 *This method checks whether two LPs have the same distance in abs value
 * to the skewness, if so the best LP in terms of migration score is chosen
 * @param lp The lp index belonging to the current eligible for migration one
 * @param other_lp Another lp index belonging to the previous eligible for migration one not yet rebound
 * @param skewness The halved skewness between the max loaded node's and the min loaded node's unbalance index
 * @return The index of the best lp to migrate
 */
unsigned int check_for_parity(unsigned int lp, unsigned int other_lp, simtime_t skewness) {

	/// initial condition
	if (other_lp == 0) return lp;

#if VERBOSE == 1
	printf("check_for_parity lp %u other_lp %u\n", LPS[lp]->lid, LPS[other_lp]->lid);

	printf("migration_scores lp %f other_lp %f\n", LPS[lp]->migration_score, LPS[other_lp]->migration_score);
	printf("distance from skewness lp %f other_lp %f\n", LPS[lp]->migration_score - skewness, LPS[other_lp]->migration_score - skewness);
#endif

	/// check on the abs value of the distance of the migration scores from the skewness
	if (fabs(LPS[lp]->migration_score - skewness) == fabs(LPS[other_lp]->migration_score - skewness)) {
		/// choose the best lp in terms of migration score in case of parity
		if (LPS[lp]->migration_score > LPS[other_lp]->migration_score) {
			return lp;
		} else {
			return other_lp; 
		}
	}

	return lp;
}


/**
 *This method chooses a subset of LPs from the max loaded numa node to be logically rebound
 * to the min loaded numa node and sets the relative bits of the dest node's bitmap
 * @param max_load_numa_node The max loaded numa node, source of the rebinding
 * @param min_load_numa_node The min loaded numa node, destination of the rebinding
 * @param skewness The skewness between the two nodes' unbalance to choose the lp to rebind
 */
void rebind_lp_to_numa_nodes(lps_to_migrate *lps_to_migrate_array, numa_struct *max_load_numa_node, numa_struct *min_load_numa_node, simtime_t skewness, double scale) {


	unsigned int lp_max_loaded_node, elem_count = 0;
	double old_skew, new_skew;
	for (unsigned int i = 0; i < n_prc_tot; i++) {
		lp_max_loaded_node = (unsigned int) get_bit(max_load_numa_node->numa_binding_bitmap, i);
		if (lp_max_loaded_node)// && eligible_for_migration(i, skewness/2.0)) {
			set_lp_to_migrate(&(lps_to_migrate_array[elem_count++]), LPS[i]->lid, LPS[i]->migration_score/scale , max_load_numa_node->unbalance_index/scale, min_load_numa_node->unbalance_index/scale);
		//else
		//	set_lp_to_migrate(&(lps_to_migrate_array[elem_count]), UNDEFINED_LP, max_load_numa_node->unbalance_index/scale , max_load_numa_node->unbalance_index/scale, min_load_numa_node->unbalance_index/scale);
		//elem_count++;
#if VERBOSE == 1
			printf("i %u- lps_to_migrate_array[i] %u - %f\n",i, lps_to_migrate_array[i].lp_idx, lps_to_migrate_array[i].expected_skewness);
#endif
	}

#if VERBOSE == 1
	printf("BEFORE SORTING %u\n", elem_count);
	for (unsigned int i = 0; i < elem_count; ++i) {
        printf("thread %u: |%u | %f | %f | %f |\t", tid, lps_to_migrate_array[i].lp_idx, 
        	lps_to_migrate_array[i].migration_score,
        	lps_to_migrate_array[i].expected_skewness,
        	 skewness/scale);
    	printf("\n");
    }
#endif


    /// sort the lps struct
	sort_lp_to_migrate(lps_to_migrate_array, elem_count);

#if VERBOSE == 1
	printf("AFTER SORTING %u\n", elem_count);
	for (unsigned int i = 0; i < elem_count; ++i) {
        printf("thread %u: |%u | %f | %f | %f |\t", tid, lps_to_migrate_array[i].lp_idx, 
        	lps_to_migrate_array[i].migration_score,
        	lps_to_migrate_array[i].expected_skewness,
        	 skewness/scale);
    	printf("\n");
    }
#endif
  #if ENFORCE_LOCALITY == 1
    int flushed_pipe = 0;
  #endif
    
    double current_delta = 0.0;
    /// do the migration of as many lps in lps_to_migrate_array as possible
	for (unsigned int j = 0; j < elem_count;j++) {
		/// check stop condition for rebinding and migration
		if(skewness/scale + current_delta < BALANCE_OVERFLOW) break;
      #if ENFORCE_LOCALITY == 1
		if(!flushed_pipe){
		 flush_locked_pipe();
		 flushed_pipe = 1;
		}
      #endif
		//printf("old skew %f\n", skewness/scale + current_delta);
		old_skew = skewness/scale + current_delta;
		current_delta -= 2*lps_to_migrate_array[j].migration_score;
		new_skew = skewness/scale + current_delta;
		printf("Migrating %d from %d to %d score:%f exp_skew:%f new_skew:%f old_skew:%f\n", lps_to_migrate_array[j].lp_idx, max_load_numa_node->numa_id, min_load_numa_node->numa_id,
        	lps_to_migrate_array[j].migration_score,
        	lps_to_migrate_array[j].expected_skewness, new_skew, old_skew);
		//printf("new skew %f\n", skewness/scale + current_delta);
		reset_bit(max_load_numa_node->numa_binding_bitmap, lps_to_migrate_array[j].lp_idx); /// reset bit from src node
		set_bit(min_load_numa_node->numa_binding_bitmap, lps_to_migrate_array[j].lp_idx); /// set bit to dest node
		LPS[j]->numa_node 				=  min_load_numa_node->numa_id;
		migrate_segment(lps_to_migrate_array[j].lp_idx, min_load_numa_node->numa_id);
	}

}


/**
 *This method plans the steps to pursue a migration of LPs from one numa node to another.
 * It chooses two numa nodes ad the source and destination of the migration. 
 * It then chooses a subset of LPs to be rebound to the destination numa node and does the rebinding.
 * It does the actual migration and resets some metrics.
 * @param numa_state The numa state of all nodes
 */
static inline void plan_and_do_lp_migration(numa_struct **numa_state) {
	numa_struct *max_load_numa_node, *min_load_numa_node;
	lps_to_migrate *lps_to_migrate_array = malloc(sizeof(lps_to_migrate) * n_prc_tot); //TODO: size of allocation
	double scale = 0.0;

	/// set the unbalance index of every numa node
	set_unbalance_index(numa_state);
	for(unsigned int i=0;i<num_numa_nodes;i++){
		scale += numa_state[i]->unbalance_index;
	}
	/// choose the numa nodes with max load and min load
	if(isnan(scale) || isinf(scale) || scale == 0.0) return;
	choose_target_nodes(numa_state, &max_load_numa_node, &min_load_numa_node);
	/// compute the skewness between the nodes
	simtime_t load_skew = max_load_numa_node->unbalance_index - min_load_numa_node->unbalance_index;
  #if VERBOSE == 0
	printf("NUMA skew:");
	for(unsigned int i=0;i<num_numa_nodes;i++){
		printf("%d:%f ", i, numa_state[i]->unbalance_index/scale);
	}
	printf("\n");
  #endif
	if(load_skew/scale < BALANCE_OVERFLOW) 
		goto out;

	//load_skew = 0.8; //just for testing on uma machine
	printf("MAX %d MIN %d SKEW %f\n", max_load_numa_node->numa_id, min_load_numa_node->numa_id, load_skew/scale);
	/// do a logical rebinding of lps to numa nodes
  #if MBIND == 1 && NUMA_REBALANCE == 1
	rebind_lp_to_numa_nodes(lps_to_migrate_array, max_load_numa_node, min_load_numa_node, load_skew, scale);
  #endif
out:
	for(unsigned int i=0;i<num_numa_nodes;i++)
		numa_state[i]->unbalance_index = 0;
	rounds_before_unbalance_check = 0;
	clock_timer_value(time_interval_for_numa_rebalance);
	free(lps_to_migrate_array);

}

#endif
