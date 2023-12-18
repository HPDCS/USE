/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file segment.c
* @brief Segment Allocator
* @author Alessandro Pellegrini
* @author Francesco Quaglia
*/
#include "mm.h"
#include "segment.h"

#include <fcntl.h>
#include <sys/types.h>
#include <numaif.h>
#include <errno.h>
#include <numaif.h>

#include <lpm_alloc.h>
#include <thr_alloc.h>
#include <platform.h>

#define LID_t unsigned int
#define GID_t unsigned int
#define lid_to_int(x) (x)
#define gid_to_int(x) (x)
#define GidToLid(c) (c)
#define n_prc pdes_configuration.ncores
#define set_gid(a,b) do{a=b;}while(0)
#define kid 1
#define GidToKernel(gid) gid 

//TODO: document this magic! This is related to the pml4 index intialized in the ECS kernel module
static unsigned char *init_address = (unsigned char *)(10LL << 39);

void *get_base_pointer(GID_t gid){
//	printf("get base pointer for lid % d (gid %d) returns: %p\n",GidToLid(gid),gid,init_address + PER_LP_PREALLOCATED_MEMORY * gid);
	return init_address + PER_LP_PREALLOCATED_MEMORY * gid_to_int(gid);
}

void *get_segment(GID_t gid, unsigned int numa_node, void ***pages) {
	unsigned long long i,j,h=0;
	void *the_address;
	void *mmapped[NUM_MMAP];
  
	/// Addresses are determined in the same way across all kernel instances
	the_address = init_address + PER_LP_PREALLOCATED_MEMORY * gid_to_int(gid);
	*pages = lpm_alloc(PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE*sizeof(void*));

	for(i = 0; i < NUM_MMAP; i++) {
		mmapped[i] = mmap(the_address, MAX_MMAP, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS /*|MAP_FIXED*/, 0, 0);

		if(mmapped[i] == MAP_FAILED) {
			rootsim_error(true, "Unable to mmap LPs memory\n");
			return NULL;
		}

		if(mmapped[i] != the_address){			
			rootsim_error(true, "Unable to mmap LPs memory on a contiguos address space\n");
			return NULL;
		}


	  if(pdes_config.enable_mbind){
		unsigned long numa_mask = 0x1 << (numa_node); 
		if(mbind(mmapped[i], MAX_MMAP, MPOL_BIND, &numa_mask, num_numa_nodes+1, MPOL_MF_MOVE) == -1){
			printf("Failing NUMA binding LP %u to node %u, with mask %p. Please retry disabling the NUMA partitioning: %s.\n",
			gid, numa_node, (void *)numa_mask, strerror(errno));
			abort();
		}
	  }
		// Access the memory in write mode to force the kernel to create the page table entries
		*((char *)mmapped[i]) = 'x';
		
		for(j=0;j<MAX_MMAP/PAGE_SIZE;j++,h++)
			(*pages)[h] = (char *)the_address+h*PAGE_SIZE;

		the_address = (char *)the_address + MAX_MMAP;


	}

	return mmapped[0];
}


void migrate_segment(unsigned int id, int numa_node){
	int *nodes, *status;
	void** lp_pages = pages[id];
	nodes  = thr_alloc(PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE*sizeof(int));
	status = thr_alloc(PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE*sizeof(int));
	for(unsigned long long i=0;i<PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;i++)
		nodes[i] = numa_node;
	move_pages(0, PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE, lp_pages, nodes, status, MPOL_MF_MOVE);
	thr_free(nodes);
	thr_free(status);
}


/*
 * TODO: this should reconstruct the addresses similarly to what is done in get_segment. Anyhow, this is called at simulation shutdown and doesn't cause much harm if it's not called.
 */
/*
 * void segment_allocator_fini(unsigned int sobjs){
	unsigned int i;
	int return_value;
	for(i=0;i<sobjs;i++){
		return_value = munmap(mem_region[i].base_pointer,PER_LP_PREALLOCATED_MEMORY);
		if(return_value){
			printf("ERROR on release value:%d\n",return_value);
			break;
		}
		mem_region[i].base_pointer = NULL;
	}
	close(ioctl_fd);

}
*/


