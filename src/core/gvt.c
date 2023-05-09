#include <simtypes.h>
#include <hpdcs_utils.h>

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


unsigned int update_fossil_gvt(){
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