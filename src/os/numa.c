#include <os/numa.h>
#include <stdio.h>
#include <stdbool.h>

extern bool numa_available_bool;

void numa_init(void){
	//TODO: make it as macro if possible
	if(numa_available() != -1){
		printf("NUMA machine with %u nodes.\n", (unsigned int)  numa_num_configured_nodes());
		numa_available_bool = true;
		num_numa_nodes = numa_num_configured_nodes();
	}
	else{
		printf("UMA machine.\n");
		numa_available_bool = false;
		num_numa_nodes = 1;
	}	

	
	/// bitmask for each numa node
	struct bitmask *numa_mask[num_numa_nodes];

	if (num_numa_nodes > 1) {

	    for (unsigned int i=0; i < num_numa_nodes; i++) {
	            numa_mask[i] = numa_allocate_cpumask();
	            numa_node_to_cpus(i, numa_mask[i]);
	    }
	}

	/// set a global array in which are set the cpus on the same numa node
	/// indexed from 0 to cpu_per_node and from cpu_per_node to N_CPU-1
	int insert_idx = 0;
	for (unsigned int j=0; j < num_numa_nodes; j++) {
		for (int k=0; k < N_CPU; k++) {
			if (num_numa_nodes ==1 || numa_bitmask_isbitset(numa_mask[j], k)) {
				cores_on_numa[insert_idx] = k; 
				insert_idx++;
			}
		}
	}
}


