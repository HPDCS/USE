#ifndef __METRICS_FOR_LP_MIGRATION_H
#define __METRICS_FOR_LP_MIGRATION_H


/**
 * This file contains the methods for metrics computation regarding
 * LPs for numa migration
 */


extern void aggregate_metrics_for_lp_migration(unsigned int current_lp, simtime_t current_evt_timestamp, simtime_t bound_timestamp, simtime_t execute_time);
extern bool compare_migration_scores(unsigned int current_lp, unsigned int lp_to_migrate);

#endif