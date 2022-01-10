#ifndef __STACK_
#define __STACK_

typedef struct _stack_node{
	struct _stack_node * volatile next;
	void *payload;
}nb_stack_node_t;


typedef struct _stack{
	nb_stack_node_t * volatile top;
}nb_stack_t;



static inline void nb_push(volatile nb_stack_t *s, nb_stack_node_t *new){
	nb_stack_node_t *top;

	do{
		top = s->top;
		new->next = top;
	}
	while(!__sync_bool_compare_and_swap(&s->top, top, new));
}

static inline nb_stack_node_t* nb_popAll(volatile nb_stack_t *s){
	return __sync_lock_test_and_set(&s->top, NULL);
}

#endif