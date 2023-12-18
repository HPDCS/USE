#ifndef __OS_NUMA_H__
#define __OS_NUMA_H__

#define _GNU_SOURCE
#include <sched.h>
#include <numa.h>

extern unsigned int num_numa_nodes;
extern __thread unsigned int current_numa_node;
extern unsigned int cores_on_numa[N_CPU];

extern void numa_init(void);

static inline unsigned int get_current_numa_node(void){
	if(numa_available() != -1){
		return numa_node_of_cpu(sched_getcpu());
	}
	return 1;
}

#endif