#include <core.h>

#include <lp/lp.h>
#include <lp/lp_lock.h>
#include <utils/compiler.h>
#include <statistics/statistics.h>

__thread simtime_t local_gvt = 0; /// Current gvt read by an individual thread
volatile simtime_t gvt = 0; /// Current global gvt
volatile simtime_t fossil_gvt = 0; /// Current global gvt

unsigned int update_global_gvt(simtime_t local_gvt){
	simtime_t cur_gvt = gvt;
	if (local_gvt > gvt) {
		return __sync_bool_compare_and_swap(
			UNION_CAST(&(gvt), unsigned long long *),
			UNION_CAST(cur_gvt,unsigned long long),
			UNION_CAST(local_gvt, unsigned long long)
		);
	}
    return false;
}


unsigned int update_fossil_gvt(void){
	simtime_t cur_gvt = fossil_gvt;
	if (gvt > cur_gvt) {
		return __sync_bool_compare_and_swap(
			UNION_CAST(&(fossil_gvt), unsigned long long *),
			UNION_CAST(cur_gvt,unsigned long long),
			UNION_CAST(gvt, unsigned long long)
		);
	}
    return false;
}


//#define print_event(event)	printf("   [LP:%u->%u]: TS:%f TB:%u EP:%u IS_VAL:%u \t\tEvt.ptr:%p Node.ptr:%p\n",event->sender_id, event->receiver_id, event->timestamp, event->tie_breaker, event->epoch, is_valid(event),event, event->node);


void check_OnGVT(unsigned int lp_idx, simtime_t ts, unsigned int tb){
	unsigned int old_state;
	current_lp = lp_idx;
	LPS[lp_idx]->until_ongvt = 0;
	
	//if(!is_end_sim(lp_idx))
    {
		//Restore state on the commit_horizon
		old_state = LPS[lp_idx]->state;
		LPS[lp_idx]->state = LP_STATE_ONGVT;
		//printf("%d- BUILD STATE FOR %d TIMES GVT START LVT:%f commit_horizon:%f\n", current_lp, LPS[current_lp]->until_ongvt, LPS[current_lp]->current_LP_lvt, LPS[current_lp]->commit_horizon_ts);
		rollback(lp_idx, ts, tb);
        //statistics_post_lp_data(lp_idx, STAT_ROLLBACK, 1);
		//printf("%d- BUILD STATE FOR %d TIMES GVT END\n", current_lp );
		
		//printf("[%u]ONGVT LP:%u TS:%f TB:%llu\n", tid, lp_idx,LPS[lp_idx]->commit_horizon_ts, LPS[lp_idx]->commit_horizon_tb);
		if(OnGVT(lp_idx, LPS[lp_idx]->current_base_pointer)){
			//printf("Simulation endend on LP:%u\n", lp_idx);
			start_periodic_check_ongvt = 1;
			end_sim(lp_idx);
		}
		// Restore state on the bound
		LPS[lp_idx]->state = LP_STATE_ROLLBACK;
		//printf("%d- BUILD STATE AFTER GVT START LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
        //statistics_post_lp_data(lp_idx, STAT_ROLLBACK, 1);
		rollback(lp_idx, INFTY, UINT_MAX);
		//printf("%d- BUILD STATE AFTER GVT END LVT:%f\n", current_lp, LPS[current_lp]->current_LP_lvt );
		LPS[lp_idx]->state = old_state;

		statistics_post_lp_data(lp_idx, STAT_ONGVT, 1);
	}
}



void round_check_OnGVT(){
	unsigned int start_from = (pdes_config.nprocesses / pdes_config.ncores) * tid;
	unsigned int curent_ck = start_from;
	
	
	//printf("OnGVT: la coda è vuota!\n");
	
	do{
		if(!is_end_sim(curent_ck)){
			if(tryLock(curent_ck)){
				//commit_horizon_ts = 5000;
				//commit_horizon_tb = 100;
				if(LPS[curent_ck]->commit_horizon_ts>0){
//					printf("[%u]ROUND ONGVT LP:%u TS:%f TB:%llu\n", tid, curent_ck,LPS[curent_ck]->commit_horizon_ts, LPS[curent_ck]->commit_horizon_tb);
	
					check_OnGVT(curent_ck, LPS[curent_ck]->commit_horizon_ts, LPS[curent_ck]->commit_horizon_tb);
				}
				unlock(curent_ck);
				break;
			}
		}
		curent_ck = (curent_ck + 1) % pdes_config.nprocesses;
	}while(curent_ck != start_from);
}