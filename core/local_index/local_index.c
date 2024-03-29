#if ENFORCE_LOCALITY == 1
#include "local_index.h"


nb_stack_node_t* nb_popAll(volatile nb_stack_t *s){
    return __sync_lock_test_and_set(&s->top, NULL);
}

void local_clean_prefix(LP_state *lp_ptr){
    local_index_t *local_idx_ptr = &lp_ptr->local_index;
    nb_stack_t *actual_idx_ptr   = &local_idx_ptr->actual_index;
    nb_stack_node_t *prev  = actual_idx_ptr->top;
    msg_t *pre_evt;

    do{
        if(prev == NULL) return;

        // this loop starts from head and disconnect each node that is not worth executing it
        while(prev!=NULL){
            pre_evt = prev->payload;
            if(pre_evt->state != ELIMINATED && pre_evt->monitor!=EVT_SAFE && pre_evt->monitor!=EVT_BANANA) return;
            actual_idx_ptr->top = prev->lnext[0];
            node_free((nbc_bucket_node*)prev);
            prev = actual_idx_ptr->top;             
        }
    }while(prev == NULL);

}


void local_remove_minimum(LP_state *lp_ptr){
    local_index_t *local_idx_ptr = &lp_ptr->local_index;
    nb_stack_t *actual_idx_ptr   = &local_idx_ptr->actual_index;
    nb_stack_node_t *tmp;

    if(actual_idx_ptr->top != NULL){
        tmp = actual_idx_ptr->top;
        actual_idx_ptr->top = tmp->lnext[0];
        node_free((nbc_bucket_node*)tmp);
    }
}

void local_ordered_insert(LP_state *lp_ptr, nb_stack_node_t *node){
    local_index_t *local_idx_ptr = &lp_ptr->local_index;
    nb_stack_t *actual_idx_ptr   = &local_idx_ptr->actual_index;
    nb_stack_node_t *cur = actual_idx_ptr->top;
    msg_t *cur_evt;

    nb_stack_node_t *prev  = cur;
    msg_t *pre_evt;

    msg_t *evt = node->payload;
    node->lnext[0] = NULL;

    do{
        if(prev == NULL){
            actual_idx_ptr->top = node;
            return;
        }

        cur = NULL;
        cur_evt = NULL;
        pre_evt = prev->payload;

        while(prev!=NULL){
            pre_evt = prev->payload;
            if( pre_evt->state   != ELIMINATED && 
                pre_evt->monitor != EVT_SAFE   && 
                pre_evt->monitor!=EVT_BANANA)
              break;
            actual_idx_ptr->top = prev->lnext[0];
            node_free((nbc_bucket_node*)prev);
            prev = actual_idx_ptr->top;             
        }
    }while(prev == NULL);

    if(BEFORE(evt, pre_evt)){
        actual_idx_ptr->top = node;
        node->lnext[0] = prev;
        return;
    }


    cur = prev->lnext[0];
    while(cur != NULL){
        cur_evt = cur->payload;
        if( cur_evt->state  != ELIMINATED && 
            cur_evt->monitor!= EVT_SAFE   && 
            cur_evt->monitor!= EVT_BANANA){

            if(BEFORE(evt, cur_evt)){
                break;
            }   
            
        }
        else{
            prev->lnext[0] = cur->lnext[0];
            node_free((nbc_bucket_node*)cur);
            cur = prev->lnext[0];
            continue;
        }

        prev = cur;
        pre_evt = cur_evt;
        cur  = cur->lnext[0];
    }

  #if DEBUG == 1
    if(prev == NULL) gdb_abort;
    if(BEFORE(evt, pre_evt)) gdb_abort;
  #endif

    prev->lnext[0] = node;
    if(cur == NULL) return;

  #if DEBUG == 1
    if(BEFORE(cur_evt, evt)) gdb_abort;
  #endif
    node->lnext[0] = cur;

    return;
}


// THIS WILL BE PUBLIC
inline void nb_push(LP_state *ptr, msg_t *evt)
{
    local_index_t *li = &ptr->local_index;
    nb_stack_node_t *new = (nb_stack_node_t*) node_malloc(NULL, 0, 0);
    new->payload = evt;

    volatile nb_stack_t *s = &li->input_channel;
    nb_stack_node_t *top;

    do{
        top = s->top;
        new->next = top;
    }
    while(!__sync_bool_compare_and_swap(&s->top, top, new));
}


// THIS WILL BE PUBLIC
inline int process_input_channel(LP_state *ptr){
  #if DEBUG == 1
    assertf(!haveLock(ptr->lid), "trying to process an input_channel of lp %u without holding its lock\n", ptr->lid);
  #endif
    msg_t *min_evt = NULL;
    nb_stack_node_t *tmp, *top =  nb_popAll(&ptr->local_index.input_channel);
    int res = 0;
    while(top){
        msg_t *evt = top->payload;
        tmp = top;
        top = top->next;
        if(evt->monitor == EVT_SAFE || evt->monitor == EVT_BANANA){
            node_free((nbc_bucket_node*)tmp);
            continue;
        }
        if(min_evt == NULL || BEFORE(evt, min_evt)) min_evt = evt;
        local_ordered_insert(ptr, tmp);
        res = 1;
    }
    local_clean_prefix(ptr);
    return res;
}
#endif
