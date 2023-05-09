#ifndef __GVT_H__
#define __GVT_H__

unsigned int update_global_gvt(simtime_t local_gvt);
unsigned int update_fossil_gvt();

extern volatile simtime_t fossil_gvt;

#endif