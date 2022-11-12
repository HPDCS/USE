#ifndef __LOCAL_INDEX_TYPES__
#define __LOCAL_INDEX_TYPES__


#define LOCAL_INDEX_LEVELS 6

typedef struct _stack_node{
	struct _stack_node * volatile next;
	struct _stack_node * lnext[LOCAL_INDEX_LEVELS];
	void *payload;
	char lvl;
}nb_stack_node_t;


typedef struct _stack{
	nb_stack_node_t * volatile top;
}nb_stack_t;


typedef struct _local_index
{
	volatile nb_stack_t input_channel;
	nb_stack_t actual_index[LOCAL_INDEX_LEVELS];	
}local_index_t;


#define actual_index_top  local_index.actual_index[0].top

#endif