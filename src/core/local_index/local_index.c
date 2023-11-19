#include "local_index.h"


#define VERBOSE_SL 0

#if VERBOSE_SL == 1
#define SL_LOG(x, ...) printf(__VA_ARGS__)
#else
#define SL_LOG(x, ...) do{}while(0)
#endif

static __thread struct drand48_data __sl_seed;
static __thread int __init_seed = 0;

nb_stack_node_t* nb_popAll(volatile nb_stack_t *s){
    return __sync_lock_test_and_set(&s->top, NULL);
}

void local_clean_prefix(LP_state *lp_ptr){
    local_index_t *local_idx_ptr = &lp_ptr->local_index;
    nb_stack_t *actual_idx_ptr   = local_idx_ptr->actual_index;
    nb_stack_node_t *prev  = actual_idx_ptr[0].top;
    msg_t *pre_evt;
    
    do{
        if(prev == NULL) return;

        // this loop starts from head and disconnect each node that is not worth executing it
        while(prev!=NULL){
            pre_evt = prev->payload;
            if(    pre_evt->state != ELIMINATED 
                && pre_evt->monitor!=EVT_SAFE 
                && pre_evt->monitor!=EVT_BANANA) return;

            for(int i=0;i<prev->lvl+1;i++){
                if(1 || prev->lnext[i]){
                  if(prev->lvl < i) gdb_abort;
                    actual_idx_ptr[i].top = prev->lnext[i];
                }
            }
            SL_LOG("clean from LP %p ts %p %f lvl:%d\n", lp_ptr, prev, prev->payload->timestamp, prev->lvl);
            for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
                SL_LOG("%p\n", actual_idx_ptr[i].top);
            }
            SL_LOG("-----------------------\n");
            node_free((nbc_bucket_node*)prev);
            prev = actual_idx_ptr[0].top;             
        }
    }while(prev == NULL);
    SL_LOG("\n");

}


void local_remove_minimum(LP_state *lp_ptr){
    local_index_t *local_idx_ptr = &lp_ptr->local_index;
    nb_stack_t *actual_idx_ptr   = local_idx_ptr->actual_index;
    nb_stack_node_t *tmp;

    if(actual_idx_ptr[0].top != NULL){
        tmp = actual_idx_ptr->top;
        for(int i=0;i<tmp->lvl+1;i++){
            if(1 || tmp->lnext[i]){
                if(tmp->lvl < i) gdb_abort;
                actual_idx_ptr[i].top = tmp->lnext[i];
            }
        }
        SL_LOG("removed from LP %p ts %p %f\n", lp_ptr, tmp, tmp->payload->timestamp);
        node_free((nbc_bucket_node*)tmp);
    }
}

void local_ordered_insert(LP_state *lp_ptr, nb_stack_node_t *node){
    long rdn;
    char lvl = -1;
    nb_stack_t *actual_idx_ptr   = lp_ptr->local_index.actual_index;
    msg_t *cur_evt;

    msg_t *evt;

    //local_clean_prefix(lp_ptr);


    // get level
    if(!__init_seed){ srand48_r(899+tid,&__sl_seed); __init_seed++;}    
    lrand48_r(&__sl_seed, &rdn);

    // init node pointers
    for(int i=0;i<LOCAL_INDEX_LEVELS;i++) node->lnext[i] = NULL;
    for(int i=0;i<LOCAL_INDEX_LEVELS;i++,lvl++) if( !(rdn & (1<<i)) ) i=LOCAL_INDEX_LEVELS;
    node->lvl = lvl;
    if(lvl < 0 || lvl >= LOCAL_INDEX_LEVELS) {printf("lvl %d\n",lvl);gdb_abort;}
    node->lvl = lvl != 0;

    evt  = node->payload;
    
    nb_stack_node_t *curs[LOCAL_INDEX_LEVELS];
    nb_stack_node_t *pres[LOCAL_INDEX_LEVELS];
    SL_LOG("\nScanning LP %p for inserting %p %f lvl:%d\n", lp_ptr, node, evt->timestamp, node->lvl);
    SL_LOG("pres curs init\n");
    for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
        pres[i] = NULL;
        curs[i] = actual_idx_ptr[i].top;
        SL_LOG("%p %p\n", pres[i], curs[i]);
    }
    SL_LOG("-----------------------\n");

    int cur_lvl = LOCAL_INDEX_LEVELS-1;
    while(cur_lvl+1){
        SL_LOG("Scanning lvl %d\n", cur_lvl);
        if(curs[cur_lvl] == NULL){
            cur_lvl--;
            continue;
        }
        cur_evt = curs[cur_lvl]->payload;
        if(AFTER_OR_CONCURRENT(evt, cur_evt)){
            pres[cur_lvl] = curs[cur_lvl];
            curs[cur_lvl] = pres[cur_lvl]->lnext[cur_lvl];
            continue;
        }
        if(BEFORE(evt, cur_evt)){
            cur_lvl--;
            if(cur_lvl < 0) continue;
            if(pres[cur_lvl+1]){
                pres[cur_lvl] = pres[cur_lvl+1];
                curs[cur_lvl] = pres[cur_lvl]->lnext[cur_lvl];
            }
            continue;
        }
    }

    for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
        SL_LOG("LVL %d - %p %p\n", i, pres[i], curs[i]);
    }
    SL_LOG("\n");

    for(int lvl=0;lvl<node->lvl+1;lvl++){
        SL_LOG("updating level %d\n", lvl);
        if(pres[lvl]){
            if(pres[lvl]->lnext[lvl] != curs[lvl]) gdb_abort;
            pres[lvl]->lnext[lvl] = node;
        }
        else
            actual_idx_ptr[lvl].top = node;
        node->lnext[lvl] = curs[lvl];
    }

    for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
        pres[i] = NULL;
        curs[i] = actual_idx_ptr[i].top;
    }
    
    SL_LOG("checking consistency\n");
    
 SL_LOG("pres curs init\n");
    for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
        pres[i] = NULL;
        curs[i] = actual_idx_ptr[i].top;
        SL_LOG("%p %p\n", pres[i], curs[i]);
    }
    SL_LOG("-----------------------\n");

    cur_lvl = LOCAL_INDEX_LEVELS-1;
    while(cur_lvl+1){
        SL_LOG("Scanning lvl %d\n", cur_lvl);
        if(curs[cur_lvl] == NULL){
            cur_lvl--;
            continue;
        }
        cur_evt = curs[cur_lvl]->payload;
        if(AFTER_OR_CONCURRENT(evt, cur_evt)){
            pres[cur_lvl] = curs[cur_lvl];
            curs[cur_lvl] = pres[cur_lvl]->lnext[cur_lvl];
            continue;
        }
        if(BEFORE(evt, cur_evt)){
            cur_lvl--;
            if(cur_lvl < 0) continue;
            if(pres[cur_lvl+1]){
                SL_LOG("skipping items\n");
                pres[cur_lvl] = pres[cur_lvl+1];
                curs[cur_lvl] = pres[cur_lvl]->lnext[cur_lvl];
            }
            continue;
        }
    }

    for(int i=0;i<LOCAL_INDEX_LEVELS;i++){
        SL_LOG("LVL %d - %p %p\n", i, pres[i], curs[i]);
    }
    SL_LOG("\n");

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
