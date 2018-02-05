#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "garbagecollector.h"
#include <hpdcs_utils.h>



__thread unsigned int ind = 0;
__thread unsigned int prune_count = 0;

typedef struct _free_list free_list;

struct _free_list
{
		linked_pointer * volatile pointer;
		//char pad[CACHE_LINE_SIZE-8];
};


#define UNROLLED_FACTOR 300
#define USE_POSIX_MEMALIGN 0
#define USE_GLOBAL_LIST 0

//static linked_pointer * volatile global_free_list = NULL;
//static char pad[CACHE_LINE_SIZE];
//static linked_pointer * volatile global_free_list_2 = NULL;

#if USE_GLOBAL_LIST == 1
static free_list global_free_list_array[8];
__thread unsigned int private_alloc_node_count = 0;
#endif

#define HEADER_SIZE 16




void* alloc_array_nodes(hpdcs_gc_status *status, unsigned int num_item)
{
	linked_gc_node *res, *tmp;
	unsigned long long res_tmp;
	// capire perchè con malloc non va e con calloc si
	res = malloc( num_item*(status->block_size +HEADER_SIZE) + CACHE_LINE_SIZE); 
	//res = calloc( 1,num_item*(status->block_size +HEADER_SIZE) + CACHE_LINE_SIZE);
	
	
	if(res == NULL)
		return NULL;
	
	res_tmp  = 	UNION_CAST(res, unsigned long long);

	if(((res_tmp & (((unsigned long long) CACHE_LINE_SIZE-1))) != 0))
	{
		res_tmp +=	CACHE_LINE_SIZE;
		res_tmp &=	~((unsigned long long) CACHE_LINE_SIZE-1);
	}
	
	tmp = UNION_CAST(res_tmp,  linked_gc_node* );
	tmp->offset_base = (unsigned int) (res_tmp - UNION_CAST(res, unsigned long long));
	tmp->next = NULL;
	res = tmp;
	res->counter = num_item;
	
	return (void*) (((char*) res)  + HEADER_SIZE);
	
} 


void free_array_nodes(hpdcs_gc_status *status, void *pointer)
{
	linked_gc_node *res = (linked_gc_node*) (((char*) pointer) - HEADER_SIZE) ;
	//free (((char*) res) - res->offset_base) ;
	
	char *tmp = (char*) pointer ;
	unsigned int num_item = res->counter;
	unsigned int i = 0;
	
	tmp += HEADER_SIZE ;
	for(;i<num_item; i++,tmp += status->block_size + HEADER_SIZE)
		mm_node_free(status, tmp) ;
	
}


void* mm_node_malloc(hpdcs_gc_status *status)
{
	linked_pointer *res;
#if USE_GLOBAL_LIST == 1
	linked_pointer *res_a;
	//linked_pointer *res_b;
#endif
	
	linked_pointer *tmp;
	linked_pointer *rolled_list_head;
	linked_pointer *free_nodes_lists 	= status->free_nodes_lists 	;
	unsigned long long res_tmp;
	//unsigned int res_pos_mem = 0;
	int rolled = UNROLLED_FACTOR;
	int i = 0;
	//int step = 200;

	
	if(free_nodes_lists == NULL)
	{	
#if USE_GLOBAL_LIST == 1
		//res_a = global_free_list;
		//res_b = global_free_list_2;
		//
		//while( res_a != NULL || res_b != NULL)
		//{
		//	if(res_a != NULL && __sync_bool_compare_and_swap(&global_free_list, res_a, res_a->next))
		//		return (void*) (((char*) res_a)  + CACHE_LINE_SIZE);
		//
		//	if(res_b != NULL && __sync_bool_compare_and_swap(&global_free_list_2, res_b, res_b->next))
		//		return (void*) (((char*) res_b)  + CACHE_LINE_SIZE);
		//	
		//	res_a = global_free_list;
		//	res_b = global_free_list_2;
		//}
		
		for(int j=0;j<16;j++)
			for(int i=0;i<8;i++)
			{
				res_a = global_free_list_array[i].pointer;
				if(res_a != NULL && __sync_bool_compare_and_swap(&global_free_list_array[i].pointer, res_a, res_a->next))
					return (void*) (((char*) res_a)  + HEADER_SIZE);
					
			}
#endif
		
#if USE_POSIX_MEMALIGN == 0 
		res = malloc( rolled*(status->block_size + HEADER_SIZE) +CACHE_LINE_SIZE);
		if(res == NULL)
#else
		res_pos_mem = posix_memalign((void**) &res, CACHE_LINE_SIZE, rolled*(status->block_size + CACHE_LINE_SIZE) );
		if(res_pos_mem != 0)
#endif		
		{
			printf("No enough memory to allocate a new node\n");
			exit(1);
		}
		res_tmp  = 	UNION_CAST(res, unsigned long long);
		
		if((res_tmp & (((unsigned long long) CACHE_LINE_SIZE-1))) != 0)
		{
			res_tmp +=	CACHE_LINE_SIZE;
			res_tmp &=	~((unsigned long long) CACHE_LINE_SIZE-1);
		}
		
		tmp = UNION_CAST(res_tmp,  linked_pointer* );
		tmp->base = res;
		tmp->next = NULL;
		res = tmp;
		
		
		#if UNROLLED_FACTOR > 1
		rolled_list_head = tmp = (linked_pointer*) (((char*) res) + status->block_size + HEADER_SIZE) ;
		for(i=1;i<UNROLLED_FACTOR-1;i++)
		{
			tmp->next = (linked_pointer*) (((char*) tmp) + status->block_size + HEADER_SIZE) ;
			tmp = tmp->next;
		}
		tmp->next = status->free_nodes_lists;
		status->free_nodes_lists = rolled_list_head;
		#endif
				
	}
	else
	{
		res = free_nodes_lists;
		status->free_nodes_lists = res->next;
	}
	
	status->to_remove_nodes_count += 1;


	return (void*) (((char*) res)  + HEADER_SIZE);
}

void mm_node_free(hpdcs_gc_status *status, void* pointer)
{
	linked_pointer *res;
#if USE_GLOBAL_LIST == 1
	linked_pointer *tmp_global;
#endif
	linked_pointer *tmp_lists 	= status->free_nodes_lists 	;
	
	
	res = (linked_pointer*) ( ((char*)pointer)- HEADER_SIZE );
	
#if USE_GLOBAL_LIST == 1
	if(private_alloc_node_count++%2 == 0)
	{
		//do
		//{
		//	tmp_global = global_free_list;
		//	res->next = tmp_global;
		//	if(__sync_bool_compare_and_swap(&global_free_list, tmp_global, res))
		//		return;
		//	
		//	tmp_global = global_free_list_2;
		//	res->next = tmp_global;
		//	
		//	if(__sync_bool_compare_and_swap(&global_free_list_2, tmp_global, res))
		//		return;
		//}while(1);
		
		for(int i=0;i<8;i++)
		{
			tmp_global = global_free_list_array[ind%8].pointer;
			res->next = tmp_global;
			if(__sync_bool_compare_and_swap(&global_free_list_array[ind++%8].pointer, tmp_global, res))
				return;
		}
	}
#endif
	res->next = tmp_lists;
	status->free_nodes_lists = (void*)res;
	
}


void mm_node_connect_to_be_freed(hpdcs_gc_status *status, void* pointer)
{
	linked_gc_node *res;
	linked_gc_node *tmp_lists 	= status->to_free_nodes 	;
	res = (linked_gc_node*) ( ((char*)pointer)- HEADER_SIZE );
	
	res->next = tmp_lists;
	status->to_free_nodes = (void*)res;
	
}


void mm_node_trash(hpdcs_gc_status *status, void* pointer,  unsigned int counter)
{
	linked_gc_node *res;
	linked_gc_node *tmp_lists 	= status->to_free_nodes 	;
	
	res = (linked_gc_node*) ( ((char*)pointer)- HEADER_SIZE );
	res->counter = counter;
	res->next = tmp_lists;
	status->to_free_nodes = res;	
}


void* mm_node_collect(hpdcs_gc_status *status, unsigned int *counter)
{
	linked_gc_node *res;
	linked_gc_node *tmp_lists 	= status->to_free_nodes_old	;
	
	if(tmp_lists == NULL)
		return NULL;
		
	res = tmp_lists;
	status->to_free_nodes_old = res->next;
	*counter = res->counter;
		
	return (void*) (((char*) res) + HEADER_SIZE);
}


void mm_new_era(hpdcs_gc_status *status, unsigned int *prune_array, unsigned int threads, unsigned int tid)
{
	unsigned int i = 0;
	
	if(status->to_free_nodes_old == NULL)
	{
		status->to_free_nodes_old = status->to_free_nodes ;
		status->to_free_nodes = NULL;
	}
	
	for(i=0;i<threads;i++)
		prune_array[(tid)*threads +i] = 0;
}


bool mm_safe(unsigned int * volatile prune_array, unsigned int threads, unsigned int tid)
{
	unsigned int i = 0;
	unsigned int flag = 1;
	if(++prune_count < 1)// TODO
		return false;

	
	prune_count = 0;

	for(;i<threads;i++)
	{
		prune_array[(tid) + i*threads] = 1;
		flag &= prune_array[(tid)*threads +i];
	}

	if(flag != 1)
		return false;
		
	
	return true;
		
}

