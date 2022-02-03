#if ENFORCE_LOCALITY == 1
#include <lp_local_struct.h>
#include <hpdcs_utils.h>
#include <fetch.h>

#define MAX_EVENTS_FROM_LOCAL_SCHEDULER 100




void local_binding_init(){

    init_struct(&thread_locked_binding);
    init_struct(&thread_unlocked_binding);
}

void local_binding_push(unsigned int lp){

    unsigned int lp_to_evict;
    int next_to_insert = thread_locked_binding.next_to_insert;
    // STEP 1. flush input channel in local index
    process_input_channel(LPS[lp]); // Fix input channel


    // STEP 2. update pipe entry
    thread_locked_binding.entries[next_to_insert].hotness                    = -1;
    thread_locked_binding.entries[next_to_insert].distance_curr_evt_from_gvt = INFTY;
    thread_locked_binding.entries[next_to_insert].distance_last_ooo_from_gvt = INFTY;


    // STEP 3. update pipe entry
    if(LPS[lp]->actual_index_top != NULL){
        msg_t *next_evt = LPS[lp]->actual_index_top->payload;
        thread_locked_binding.entries[next_to_insert].distance_curr_evt_from_gvt = next_evt->timestamp - local_gvt;
    }


    //insertion into pipe
    lp_to_evict = insert_lp_in_pipe(&thread_locked_binding, lp);

  #if VERBOSE == 1
    printf("[%u] after insertion: \n", tid);
    for (size_t j = 0; j < thread_locked_binding.size; j++) {
        printf("|%d| ", thread_locked_binding.entries[j].lp);
    }
    printf("\n");
    printf("[%u] lp_to_evict %u\n", tid, lp_to_evict);
  #endif

    if (!is_in_pipe(&thread_locked_binding, lp_to_evict)) {
        //eviction of an lp
        if (lp_to_evict != UNDEFINED_LP) {
            unlock(lp_to_evict);
            insert_lp_in_pipe(&thread_unlocked_binding, lp_to_evict);
          #if VERBOSE == 1
            printf("[%u] after eviction: \n", tid);
            for (size_t j = 0; j < thread_unlocked_binding.size; j++) {
                printf("|%d| ", thread_unlocked_binding.entries[j].lp);
            }
            printf("\n");
          #endif
        }
    }

    
}



int local_fetch(){
    unsigned int min_lp; 
    int res = 0;
    simtime_t minimum_diff;
    msg_t *min_local_evt, *local_next_evt;
    int current_state, valid, in_past;
    bool from_get_next_and_valid;
    if(local_schedule_count == MAX_EVENTS_FROM_LOCAL_SCHEDULER){
        local_schedule_count = 0;
        return res;
    }
  retry:
    res = 0;
    min_lp = UNDEFINED_LP;
    minimum_diff = INFTY;
    from_get_next_and_valid = false;

    //no local fetch if the pipe is empty
    if (thread_locked_binding.entries[0].lp == UNDEFINED_LP) return res;

    //find the best event to schedule into pipe
    detect_best_event_to_schedule(&thread_locked_binding, &min_lp, &minimum_diff, &min_local_evt);
    
    //NB if in the local pipe there is no event with distance < MAX we go in global queue

    if(min_lp != UNDEFINED_LP){
        // its time to run the get_next_and_valid

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

#endif