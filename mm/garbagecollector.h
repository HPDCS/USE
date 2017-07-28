#ifndef __HPDCS_MM__
#define __HPDCS_MM__

#include <stdlib.h>

typedef struct _linked_pointer linked_pointer;
typedef struct _linked_gc_node linked_gc_node;

struct _linked_pointer{
	linked_pointer *next;
	void* base;
};

struct _linked_gc_node{
	linked_gc_node *next;
	unsigned int counter;
	unsigned int offset_base;
};

typedef struct hpdcs_gc_status
{
	linked_pointer *free_nodes_lists 	;
	linked_gc_node *to_free_nodes 		;
	linked_gc_node *to_free_nodes_old 	;
	unsigned int block_size 			;
	unsigned int offset_next 			;
	long long to_remove_nodes_count 	;
}
hpdcs_gc_status;


void* mm_node_malloc(hpdcs_gc_status *status);
void  mm_node_free(hpdcs_gc_status *status, void* pointer);
void  mm_node_trash(hpdcs_gc_status *status, void* pointer,  unsigned int counter);
void* mm_node_collect(hpdcs_gc_status *status, unsigned int *counter);
void  mm_new_era(hpdcs_gc_status *status, unsigned int *prune_array, unsigned int threads, unsigned int tid);
void mm_node_connect_to_be_freed(hpdcs_gc_status *status, void* pointer);
void* alloc_array_nodes(hpdcs_gc_status *status, unsigned int num_item);
void free_array_nodes(hpdcs_gc_status *status, void *pointer);
bool mm_safe(unsigned int * volatile prune_array, unsigned int threads, unsigned int tid);

#endif
