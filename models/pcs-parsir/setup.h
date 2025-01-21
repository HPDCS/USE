#include <stdint.h>
#define OBJECTS (4096)
#define LOOKAHEAD (1.0)

//#include <run.h>


#ifndef MEM_NODES
#define MEM_NODES (2)
#pragma message("Number of NUMA nodes set to the default value 2")
#endif

#define STARTUP_TIME (0.0)

int GetEvent(int *, double *, int *, char* , int* );

void ProcessEvent(unsigned int , double , int , void *, unsigned int , void *ptr);

#define Random(a, b) Random()
#define Expent(x, a, b) Expent(x)
//double Expent(double, uint32_t*, uint32_t*);

//the below max values can be modified with no problem
//for handling very huge hardware platforms
#define MAX_NUMA_NODES 128
#define MAX_CPUS_PER_NODE 1024

//#define NUMA_BALANCING
#define BENCHMARKING

#define THRESHOLD 4 //number of objects gets after which TLB is flushed
			//this is relevand just for testing the cost of TLB invalidation
//#define TLB_TEST

int get_current(void);
int get_NUMAnode(void);
int get_totNUMAnodes(void);
int * getcounter(void);
int * getmin(void);
int * getmax(void);

#define UPDATE(addr,val) *addr = val //; *((char*)addr + MAX_SIZE) = val TO BE FIXED
