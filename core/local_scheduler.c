#if ENFORCE_LOCALITY == 1
#include <lp_local_struct.h>
#include <local_index/local_index.h>
#include <queue.h>
#include <hpdcs_utils.h>
#include <fetch.h>
#include <prints.h>

#define MAX_EVENTS_FROM_LOCAL_SCHEDULER 100


__thread pipe thread_locked_binding[CURRENT_BINDING_SIZE];
__thread pipe thread_unlocked_binding[CURRENT_BINDING_SIZE];
__thread local_schedule_count = 0;

__thread int next_to_insert; //next index of the array where to insert an lp
__thread int last_inserted; //last index of the array where an lp has being inserted

__thread int next_to_insert_evicted;
__thread int last_inserted_evicted;

void local_binding_init(){

    int i;
    for(i = 0; i < CURRENT_BINDING_SIZE; i++){
        thread_locked_binding[i].lp = UNDEFINED_LP;
        thread_locked_binding[i].hotness = 0;
        thread_locked_binding[i].distance_curr_evt_from_gvt = 0;
        thread_locked_binding[i].distance_last_ooo_from_gvt = 0;
        //thread_locked_binding[i].evicted = true;

        thread_unlocked_binding[i].lp = UNDEFINED_LP;
        thread_unlocked_binding[i].hotness = 0;
        thread_unlocked_binding[i].distance_curr_evt_from_gvt = 0;
        thread_unlocked_binding[i].distance_last_ooo_from_gvt = 0;
    }

    next_to_insert = 0;
    last_inserted = 0;

    next_to_insert_evicted = 0;
    last_inserted_evicted = 0;
}

void local_binding_push(unsigned int lp){

    unsigned int lp_to_evict;

    // STEP 1. flush input channel in local index
    process_input_channel(LPS[lp]); // Fix input channel


    // STEP 2. update pipe entry
    thread_locked_binding[next_to_insert].hotness                    = -1;
    thread_locked_binding[next_to_insert].distance_curr_evt_from_gvt = INFTY;
    thread_locked_binding[next_to_insert].distance_last_ooo_from_gvt = INFTY;


    // STEP 3. update pipe entry
    if(LPS[lp]->actual_index_top != NULL){
        msg_t *next_evt = LPS[lp]->actual_index_top->payload;
        thread_locked_binding[next_to_insert].distance_curr_evt_from_gvt = next_evt->timestamp - local_gvt;
    }


    //insertion into pipe
    lp_to_evict = insert_lp_in_pipe(&thread_locked_binding[next_to_insert].lp, lp, 
                                                 CURRENT_BINDING_SIZE, &last_inserted, &next_to_insert);

    #if DEBUG == 1
        printf("[%u] after insertion: \n", tid);
        for (int j = 0; j < CURRENT_BINDING_SIZE; j++) {
            printf("|%d| ", thread_locked_binding[j].lp);
        }
        printf("\n");
        printf("[%u] lp_to_evict %u\n", tid, lp_to_evict);
    #endif

    //eviction of an lp
    if (lp_to_evict != UNDEFINED_LP) {

        unlock(lp_to_evict);
        insert_lp_in_pipe(&thread_unlocked_binding[next_to_insert_evicted].lp, lp_to_evict, 
                             CURRENT_BINDING_SIZE, &last_inserted_evicted, &next_to_insert_evicted);
        #if DEBUG == 1
            printf("[%u] after eviction: \n", tid);
            for (int j = 0; j < CURRENT_BINDING_SIZE; j++) {
                printf("|%d| ", thread_unlocked_binding[j].lp);
            }
            printf("\n");
        #endif
    }


    
}


/* The function finds the next event from one lp 
@param : index The index of the current pipe field to check
@param : cur_lp The index of the lp being processed
@param : current_lp_distance The distance of next event from local gvt to update
It returns the event to be analyzed
*/
msg_t * find_next_event_from_lp(unsigned *cur_lp, simtime_t *current_lp_distance, simtime_t *distance_curr_evt_from_gvt) {


    // flush input channel
    process_input_channel(LPS[*cur_lp]);

    // get the next event and compute its distance from current_lvt
    msg_t *next_evt = LPS[*cur_lp]->actual_index_top->payload;
    *current_lp_distance = next_evt->timestamp - local_gvt;

    *distance_curr_evt_from_gvt = *current_lp_distance;

    return next_evt;

}


int local_fetch(){
    unsigned int cur_lp, min_lp, best_lp; 
    int i, j, res = 0;
    simtime_t minimum_diff, current_lp_distance, last_inserted_distance, best_diff;
    msg_t *next_evt, *last_inserted_evt, *min_local_evt, *local_next_evt;
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
    if (thread_locked_binding[0].lp == UNDEFINED_LP) return res;

    //get last inserted lp as the best one to compare with others
    best_lp = thread_locked_binding[last_inserted].lp;
    last_inserted_evt = find_next_event_from_lp(&best_lp, &last_inserted_distance, 
                &thread_locked_binding[last_inserted].distance_curr_evt_from_gvt);

    if (last_inserted_distance < MAX_LOCAL_DISTANCE_FROM_GVT) {
            min_lp = best_lp;
            minimum_diff = best_diff;
            min_local_evt = last_inserted_evt;
    }
    

    //iterate over pipe from the last inserted lps 
    i = last_inserted == 0 ? CURRENT_BINDING_SIZE-1 : last_inserted;
    j = 0;
    while (i+j != last_inserted) {


        //get lp
        cur_lp = thread_locked_binding[i+j].lp;

        if (cur_lp == UNDEFINED_LP) break;

        //find the next event for the current lp
        next_evt = find_next_event_from_lp(&cur_lp, &current_lp_distance, &thread_locked_binding[i+j].distance_curr_evt_from_gvt);

        #if DEBUG == 1
            printlp("[%u]BEST_LP : %u ", tid, best_lp);printf("\t last_inserted_distance %f\n", last_inserted_distance);
            printlp("[%u]CUR_LP : %u ", tid, cur_lp);printf("\t current_lp_distance %f\n", current_lp_distance);
        #endif

        //if best_lp already satisfies the condition skip this and just flush events into local index of other lps
        if(min_lp != best_lp && current_lp_distance < MAX_LOCAL_DISTANCE_FROM_GVT){ //&& current_lp_distance < minimum_diff){
            
            min_lp       = thread_locked_binding[i+j].lp;
            minimum_diff = thread_locked_binding[i+j].distance_curr_evt_from_gvt;
            min_local_evt= next_evt;
            
        }

        if (i+j == 0) j = CURRENT_BINDING_SIZE-1;
        i--;
        
    }


    //NB if in the local pipe there is no event with distance < MAX we go in global queue

    #if DEBUG == 1
            printlp("[%u] MIN_LP : %u\n", tid, min_lp);
    #endif

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