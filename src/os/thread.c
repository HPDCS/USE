#define _GNU_SOURCE
#include <sched.h>
#include <os/numa.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

extern __thread unsigned int current_cpu;


void set_affinity(unsigned int tid){
	unsigned int cpu_per_node;
	cpu_set_t mask;
	cpu_per_node = N_CPU/num_numa_nodes;
	
	current_cpu = ((tid % num_numa_nodes) * cpu_per_node + (tid/((unsigned int)num_numa_nodes)))%N_CPU;

	CPU_ZERO(&mask);
	CPU_SET(cores_on_numa[current_cpu], &mask);

	current_cpu = cores_on_numa[current_cpu];
	
	int err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	if(err < 0) {
		printf("Unable to set CPU affinity: %s\n", strerror(errno));
		exit(-1);
	}
	
	current_numa_node = get_current_numa_node();
	printf("Thread %2u set to CPU %2u on NUMA node %2u\n", tid, cores_on_numa[current_cpu], current_numa_node);
	
}