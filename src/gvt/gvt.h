#ifndef __GVT_H__
#define __GVT_H__

extern unsigned int update_global_gvt(simtime_t local_gvt);
extern unsigned int update_fossil_gvt(void);
extern void check_OnGVT(unsigned int lp_idx, simtime_t ts, unsigned int tb);
extern void round_check_OnGVT();

extern volatile simtime_t fossil_gvt;

#endif