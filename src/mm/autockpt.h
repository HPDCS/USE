#define AUTOCKPT_EMA 0.3
#define AUTOCKPT_PER 2

static inline void autockpt_update_interval(unsigned int lp){
	double a = 2.0 * LPS[lp]->ema_take_snapshot_time;
	double b = LPS[lp]->ema_rollback_probability * LPS[lp]->ema_silent_event_granularity;
	double res = ceil(sqrt(a/b));
	unsigned int ires = (unsigned int)res;
	
	if(res > 0){
	//		 if(ires > LPS[lp]->ckpt_period) LPS[lp]->ckpt_period++;
	//	else if(ires < LPS[lp]->ckpt_period) LPS[lp]->ckpt_period--; 
	}	
	if(ires > 1){
		//printf("LP %u : CHECKPOINT pre %u new %u\n", lp, LPS[lp]->ckpt_period, ires);
		LPS[lp]->ckpt_period = ires;
	}
}

static inline void autockpt_update_consecutive_forward_count(unsigned int lp){
	LPS[lp]->consecutive_forward_count++;
}

static inline void autockpt_update_ema_silent_event(unsigned int lp, unsigned long long granularity){
	LPS[lp]->ema_silent_event_granularity = LPS[lp]->ema_silent_event_granularity*(1-AUTOCKPT_EMA) + granularity*(AUTOCKPT_EMA); 	
}

static inline void autockpt_update_ema_full_log(unsigned int lp, unsigned long long granularity){
	LPS[lp]->ema_take_snapshot_time = LPS[lp]->ema_take_snapshot_time*(1-AUTOCKPT_EMA) + granularity*(AUTOCKPT_EMA); 	
}

static inline void autockpt_update_ema_rollback_probability(unsigned int lp){
	unsigned int len = LPS[lp]->consecutive_forward_count;
	LPS[lp]->consecutive_forward_count = 0;
	double prob = 1.0/len;
	LPS[lp]->ema_rollback_probability = LPS[lp]->ema_rollback_probability*(1-AUTOCKPT_EMA) + prob*(AUTOCKPT_EMA);
	LPS[lp]->consecutive_rollbacks_count++;
	if( (LPS[lp]->consecutive_rollbacks_count % AUTOCKPT_PER) == 0)
		autockpt_update_interval(lp);	
}
