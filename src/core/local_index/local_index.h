#ifndef __STACK_
#define __STACK_

#include <lp/lp.h>
#include <nb_calqueue.h>
#include <queue.h>
#include <hpdcs_utils.h>

extern nb_stack_node_t* nb_popAll(volatile nb_stack_t *s);
extern void local_clean_prefix(LP_state *lp_ptr);
extern void local_remove_minimum(LP_state *lp_ptr);
extern void local_ordered_insert(LP_state *lp_ptr, nb_stack_node_t *node);
extern void nb_push(LP_state *ptr, msg_t *evt);
extern int process_input_channel(LP_state *ptr);


#endif