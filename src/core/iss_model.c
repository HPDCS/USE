#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>

#include <ROOT-Sim.h>

#include <mm.h>
#include <dymelor.h>
#include <segment.h>

#include <incremental_state_saving.h>

lp_iss_metadata *iss_states; /// runtime iss metadata for each lp
model_t iss_costs_model;	 /// runtime tuning of the cost model 


/** methods for model's management */


float estimate_cost(size_t size, float probability){
    float cost = iss_costs_model.mprotect_cost_per_page + probability*iss_costs_model.log_cost_per_page*size;
    assert(cost>0);
	return cost;
}

void iss_first_run_model(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int i = 0;
    if(cur_lp == 0) printf("NUM PAGES:%u\n", start);
    iss_states[cur_lp].cur_virtual_ts = 1;
  #ifdef BUDDY
    partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
    for(i = 0; i<end; i++){
        tree[i].valid[0] = i>=start;
		tree[i].dirty = 0;
		tree[i].cost  = 0.0;
        tree[i].access_count = 0;
	} 
   #endif
    //tree[1].valid[0] = 1; //i>=start;
}

void iss_update_model(unsigned int cur_lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
	unsigned int size  = 1;
    unsigned int parent= 0;
    unsigned int cur_model = iss_states[cur_lp].current_model; 
	unsigned int cur_round = 0;
  #ifdef BUDDY
    partition_node_tree_t *tree = &iss_states[cur_lp].partition_tree[0]; 
  #endif
    iss_states[current_lp].iss_model_round++;
    if((iss_states[current_lp].iss_model_round%10)!=0) return;
//    return;
    //iss_states[cur_lp].cur_virtual_ts = 1;

    if(iss_states[cur_lp].disabled < 2){ 
        iss_states[cur_lp].disabled++;

    #ifdef BUDDY
        for(unsigned int i = 0; i<start; i++){
		//	tree[i].dirty = 0;
        	tree[i].valid[cur_model] = 0;
		}

		for(unsigned int i = start; i<end; i++){
		//	tree[i].dirty = 0;
			tree[i].valid[cur_model] = 1;
		} 
    #endif
    
    } ///end if disabled < 2

    if(iss_states[cur_lp].disabled == 2){
        int counts[20];
        int idx=0;
        if(cur_lp == 0)
            printf("model update\n");
        iss_states[cur_lp].disabled++;

        while(start > 1){
            size *= 2;
  #ifdef BUDDY
            for(unsigned int i = start; i<end;i+=2){
                parent = i/2;
                if(tree[parent].cost < tree[i].cost+tree[i+1].cost){
                                   tree[parent].valid[cur_model] = 1;
                }
                else{
                                   tree[parent].valid[cur_model] = 0;
                    tree[parent].cost = tree[i].cost+tree[i+1].cost;
                }
                
            } ///end for
  #endif
            end    = start;
            start /= 2; 
        } ///end while
        
        start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
        end = start*2;
        
        if( current_lp == 0){
            start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
            end = start*2;
            int j=0;
            while(start >0){
                printf("LAYER %u\n", j);
  #ifdef BUDDY
                for(unsigned int i =start; i<end;i+=2){
                    if(tree[i].cost > 0 || tree[i+1].cost > 0 ){
                        printf("\t%u: %f %u %f\n", i, tree[i].cost, tree[i].valid[0], ((float)tree[i].access_count) / ((float)iss_states[current_lp].iss_model_round));
                        if(i != 1)
                        printf("\t%u: %f %u %f\n\n", i+1, tree[i+1].cost, tree[i+1].valid[0], ((float)tree[i+1].access_count) / ((float)iss_states[current_lp].iss_model_round));
                    }
                }
  #endif
                end >>=1;
                start >>=1;
                j++;

            } ///end while

            start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
            end = start*2;

        }  ///end if lp == 0      
        
    } /// end if disabled == 2
    
}

void iss_log_incremental_reset(unsigned int lp){
	unsigned int start = PER_LP_PREALLOCATED_MEMORY/PAGE_SIZE;
	unsigned int end   = start*2;
    unsigned int cur_model = iss_states[lp].current_model; 
  #ifdef BUDDY
    partition_node_tree_t *tree = &iss_states[lp].partition_tree[0]; 
  #endif
    iss_states[lp].current_incremental_log_size = 0;
    iss_states[lp].count_tracked = 0;
    
    
    if(iss_states[lp].cur_virtual_ts == 65000){
        iss_states[lp].cur_virtual_ts = 0;

  #ifdef BUDDY
        for(unsigned int i = 0; i<end; i++){
            tree[i].dirty = 0;
        } 
  #endif
    }
    
    iss_states[lp].cur_virtual_ts += 1;
    

}