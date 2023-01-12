#include <core.h>
#include <simtypes.h>



#define ALFA_EMA 0.6 ///constant for interrival time ema
#define BETA_EMA 0.8 ///constant for granularity ema



/**
 * This method computes various metrics for LP migration
 * 
 * @param current_lp The LP index 
 * @param current_evt_timestamp The current event timestamp to compute the interarrival time difference
 * @param bound_timestamp The last corrected executed event's timestamp used to compute the interrival time difference
 * @param execute_time The execution time of the current event to compute the granularity ema
 */
void aggregate_metrics_for_lp_migration(unsigned int current_lp, simtime_t current_evt_timestamp, simtime_t bound_timestamp, simtime_t execute_time) {

    if(current_evt_timestamp < bound_timestamp) return;
    /// computation of ema_ti
    LPS[current_lp]->ema_ti = (1-ALFA_EMA)*LPS[current_lp]->ema_ti + ALFA_EMA*(current_evt_timestamp - bound_timestamp);
    
    /// computation of ema_granularity
    LPS[current_lp]->ema_granularity =  (1-BETA_EMA)*LPS[current_lp]->ema_granularity + BETA_EMA*execute_time;

}



/**
 * This method compared the migration scores of two LPs to decide whether to change the
 * LP to migrate in the future or not
 * 
 * @param current_lp The LP index of the current lp being processed
 * @param lp_to_migrate The LP index of the last saved LP as the LP to be
 * @return True if the migration score of the current LP is greater than the previously saved LP's
 */
bool compare_migration_scores(unsigned int current_lp, unsigned int lp_to_migrate) { 

	/// comparison of migration scores
	return LPS[current_lp]->migration_score > LPS[lp_to_migrate]->migration_score;

}