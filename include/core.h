#ifndef __CORE_H
#define __CORE_H

#include <stdbool.h>
#include <math.h>
#include <float.h>

#include "time_util.h"


#define MAX_LPs	2048

#define D_DIFFER_ZERO(a) (fabs(a) >= DBL_EPSILON)


extern unsigned int n_prc_tot;

void init(void);

//Esegue il loop del singolo thread
void thread_loop(unsigned int thread_id);

extern __thread simtime_t current_lvt;
extern __thread unsigned int current_lp;

extern void rootsim_error(bool fatal, const char *msg, ...);

extern void _mkdir(const char *path);

extern int OnGVT(unsigned int me, void *snapshot);

#endif
