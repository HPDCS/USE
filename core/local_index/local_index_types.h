#ifndef __LOCAL_INDEX_TYPES__
#define __LOCAL_INDEX_TYPES__


typedef struct _stack_node{
	struct _stack_node * volatile next;
	struct _stack_node * lnext[4];
	void *payload;
}nb_stack_node_t;


typedef struct _stack{
	nb_stack_node_t * volatile top;
}nb_stack_t;


typedef struct _local_index
{
	volatile nb_stack_t input_channel;
	nb_stack_t actual_index;	
}local_index_t;


#define actual_index_top  local_index.actual_index.top

#endif