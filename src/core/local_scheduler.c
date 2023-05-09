#include <lp_local_struct.h>
#include <hpdcs_utils.h>
#include <fetch.h>
#include <pthread.h>

#define MAX_EVENTS_FROM_LOCAL_SCHEDULER 100

__thread pipe_t *thread_locked_binding;
__thread pipe_t *thread_unlocked_binding;
__thread int local_schedule_count = 0;

pipe_t *evicted_pipe_pointers[HPIPE_INDEX1_LEN];

pthread_barrier_t local_schedule_init_barrier;

void local_binding_init(){
    thread_locked_binding   = init_struct(pdes_config.el_locked_pipe_size);
    thread_unlocked_binding = init_struct(pdes_config.el_evicted_pipe_size);
    evicted_pipe_pointers[current_cpu] = thread_unlocked_binding; 
    pthread_barrier_wait(&local_schedule_init_barrier);
}

void local_binding_push(unsigned int lp){

    pipe_entry_t entry_to_be_evicted;
    unsigned int lp_to_evict;
    simtime_t next_ts = INFTY;
    LPS[lp]->wt_binding = tid;

    // STEP 1. flush input channel in local index
    process_input_channel(LPS[lp]); // Fix input channel
    // STEP 2. get next ts 
    if(LPS[lp]->actual_index_top != NULL){
        msg_t *next_evt = LPS[lp]->actual_index_top->payload;
        next_ts = next_evt->timestamp;
    }

    //insertion into pipe
    lp_to_evict = insert_lp_in_pipe(thread_locked_binding, lp, next_ts, &entry_to_be_evicted);
  #if VERBOSE == 1
    printf("[%u] after insertion: \n", tid);
    for (size_t j = 0; j < thread_locked_binding->size; j++) {
        printf("|%d| ", thread_locked_binding->entries[j].lp);
    }
    printf("\n");
    printf("[%u] lp_to_evict %u\n", tid, lp_to_evict);
  #endif

    //TODO: respost metadata of the evicted LP from pipe to evicted pipe
    if(lp_to_evict != UNDEFINED_LP && lp_to_evict != lp){
        LPS[lp_to_evict]->wt_binding = UNDEFINED_WT;
        unlock(lp_to_evict);
        next_ts = entry_to_be_evicted.next_ts;
        insert_lp_in_pipe(thread_unlocked_binding, lp_to_evict, next_ts, &entry_to_be_evicted);
      #if VERBOSE == 1
        printf("[%u] after eviction: \n", tid);
        for (size_t j = 0; j < thread_unlocked_binding->size; j++) {
            printf("|%d| ", thread_unlocked_binding->entries[j].lp);
        }
        printf("\n");
      #endif
    }
}

void flush_locked_pipe(){

    pipe_entry_t entry_to_be_evicted;
    unsigned int lp_to_evict;
    simtime_t next_ts;

    for(int i=0;i<thread_locked_binding->size;i++){
        entry_to_be_evicted = thread_locked_binding->entries[i]; 
        lp_to_evict = entry_to_be_evicted.lp; 
        if(lp_to_evict == UNDEFINED_LP) continue;
        LPS[lp_to_evict]->wt_binding = UNDEFINED_WT;
        unlock(lp_to_evict);
        next_ts = entry_to_be_evicted.next_ts;
        update_pipe_entry(thread_locked_binding, i, UNDEFINED_LP, INFTY);
        insert_lp_in_pipe(thread_unlocked_binding, lp_to_evict, next_ts, &entry_to_be_evicted);
    }
}




int local_fetch(){
    unsigned int min_lp; 
    int res = 0;
    msg_t *next_evt;
    msg_t *min_local_evt, *local_next_evt;
    int current_state, valid, in_past;
    size_t i;
    bool from_get_next_and_valid;
    if(local_schedule_count == MAX_EVENTS_FROM_LOCAL_SCHEDULER){
        local_schedule_count = 0;
        return res;
    }
    MAX_LOCAL_DISTANCE_FROM_GVT = get_current_window();
  retry:
    res = 0;
    min_lp = UNDEFINED_LP;
    from_get_next_and_valid = false;

    // update local pipe
    for(i=0;i<thread_locked_binding->size;i++){
         unsigned int lp = thread_locked_binding->entries[i].lp; 
      #if DEBUG == 1
        assertf(lp != UNDEFINED_LP && !haveLock(lp), "found %u in the locked pipe without lock\n", lp);
      #endif
        if(lp == UNDEFINED_LP) continue;
        next_evt = find_next_event_from_lp(lp);
        if(!next_evt) continue;
        update_pipe_entry(thread_locked_binding, i, lp, next_evt->timestamp);
    }
    //find the best event to schedule into pipe

    min_lp = detect_best_event_to_schedule(thread_locked_binding);
    i = 0;
    while(min_lp == UNDEFINED_LP && i<HPIPE_INDEX2_LEN){
        pipe_t *pipe_to_check = evicted_pipe_pointers[hpipe_index1[current_cpu][i++]];
        if(!pipe_to_check) continue;
        min_lp = detect_best_event_to_schedule(pipe_to_check);
        if(min_lp != UNDEFINED_LP && !tryLock(min_lp)) min_lp = UNDEFINED_LP;
    }
    
    //NB if in the local pipe there is no event with distance < MAX we go in global queue

    if(min_lp != UNDEFINED_LP){
        // its time to run the get_next_and_valid
        min_local_evt = find_next_event_from_lp(min_lp);
        if(!min_local_evt) goto retry;

        valid = is_valid(min_local_evt);
        in_past = BEFORE_OR_CONCURRENT(min_local_evt, LPS[min_lp]->bound);

        if(valid){
            local_next_evt = get_next_and_valid(LPS[min_lp], LPS[min_lp]->bound);
                
            if( local_next_evt != NULL && 
                BEFORE(local_next_evt, min_local_evt)
            ){
              #if DEBUG==1
                if(local_next_evt->monitor == EVT_SAFE){ //DEBUG
                    printf("[%u]GET_NEXT_AND_VALID from local index: \n",tid); 
                    printf("\tlocal min     : ");print_event(min_local_evt);
                    printf("\tlocal_next_evt: ");print_event(local_next_evt);
                    gdb_abort;
                }
                assertf(local_next_evt->state != EXTRACTED && local_next_evt->state != ANTI_MSG, "found a not extracted event from local_queue%s", "\n");
              #endif  
                min_local_evt = local_next_evt;
                from_get_next_and_valid = true;
                current_state = EXTRACTED; 
            }
            else {
                current_state = min_local_evt->state;
            }

            if(current_state == NEW_EVT){
                if(!EVT_TRANSITION_NEW_EXT(min_local_evt)) 
                    goto retry;
            }else if(current_state == EXTRACTED){
                if(in_past) {
                    assertf(from_get_next_and_valid, "from_get_next_and_valid and in past%s", "\n");
                    local_remove_minimum(LPS[min_lp]);
                    goto retry;
                }
            }else if(current_state == ELIMINATED){
                goto retry;

            }else if(current_state == ANTI_MSG){
                if(!in_past) EVT_TRANSITION_ANT_BAN(min_local_evt);
                if(min_local_evt->monitor == EVT_BANANA) goto retry;
            }
            assertf(min_local_evt->state == NEW_EVT, "local scheduler is returning a valid NEW_EVT event %p\n", min_local_evt);
        }
        else{
            int state_before_trans = min_local_evt->state;
            current_state = state_before_trans;
            if(current_state == NEW_EVT)   {EVT_TRANSITION_NEW_ELI(min_local_evt); goto retry;}
            if(current_state == EXTRACTED)  EVT_TRANSITION_EXT_ANT(min_local_evt);
            current_state = min_local_evt->state;

            if(current_state == ELIMINATED) goto retry;
            if(current_state == ANTI_MSG){
                if(!in_past) EVT_TRANSITION_ANT_BAN(min_local_evt);   
                if(min_local_evt->monitor == EVT_BANANA)   goto retry; 
            }
            assertf(min_local_evt->state == NEW_EVT, "local scheduler is returning an invalid NEW_EVT event %p, %d\n", min_local_evt, state_before_trans);
        }



        // preprare global variable before completion
        current_msg  = min_local_evt;
        current_node = NULL; 
        assertf(current_msg->state == NEW_EVT, "local scheduler is returning a NEW_EVT event %p\n", current_msg);
        safe = false;
        res = 1;
        __event_from =2;
        local_schedule_count++;
    }
    
    return res;
}