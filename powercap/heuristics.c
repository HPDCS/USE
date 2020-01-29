
///////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////

// Checks if the current config is better than the currently best config and if that's the case update it 
void update_best_config(double throughput, double power){
	
	if(throughput > best_throughput || (best_threads == active_threads && current_pstate <= best_pstate)){
		best_throughput = throughput;
		best_pstate = current_pstate;
		best_threads = active_threads;
		best_power = power;
	}
}

int update_level_best_config(double throughput){
	if(throughput > level_best_throughput){
		level_best_throughput = throughput;
		level_best_threads = active_threads;
		level_best_pstate = current_pstate;
		return 1;
	}
	else return 0;
}

void compare_best_level_config(){
	if(level_best_throughput > best_throughput){
		best_throughput = level_best_throughput;
		best_pstate = level_best_pstate;
		best_threads = level_best_threads;
	}
}

// Stop searching and set the best configuration 
void stop_searching(){

	decreasing = 0;
	new_pstate = 1;
	stopped_searching = 1;

	level_best_throughput = 0;
	level_best_threads = 0;
	level_best_pstate = 0;

	phase0_threads = -1;
	phase0_pstate = -1;
	phase = 0;

	current_exploit_steps = 0;

	if(best_throughput == -1){
		best_threads = 1;
		best_pstate = max_pstate;
	}

	if(heuristic_mode == 15 && detection_mode == 3){
		set_pstate(validation_pstate);
		set_threads(1);
	}else{
		set_pstate(best_pstate);
		set_threads(best_threads);
    }


    //Set High and Low for fluctuations when running the model
    if((heuristic_mode == 15 || heuristic_mode == 16 || heuristic_mode == 13) && detection_mode == 2){

    	high_threads = best_threads;
    	low_threads = best_threads;
    	
    	if(best_pstate > 0)
    		high_pstate = best_pstate - 1;
    	else high_pstate = best_pstate;

    	// Must set dummy value because high was not explored
    	high_throughput = best_throughput+best_throughput*0.1;
    	high_power = best_power+best_power*0.1;

    	if(best_pstate < max_pstate)
    		low_pstate = best_pstate+1;
    	else low_pstate = best_pstate;
    	
    	// Must set dummy value because high was not explored
    	low_throughput = best_throughput+best_throughput*0.1;
    	low_power = best_power+best_power*0.1;
    }

	#ifdef DEBUG_HEURISTICS
		printf("EXPLORATION COMPLETED IN %d STEPS. OPTIMAL: %d THREADS P-STATE %d\n", steps, best_threads, best_pstate);
	#endif

	steps = 0; 
}


// Returns a configuration at the given pstate with power consumption less than the one of the current configuration 
// All the power related data is retrieved from the profiler_matrix and is only used as a starting point, as this values change based on the workload
// If no configuration at the given pstate uses less than the configuration power the algorithm returns the lowest consuming configuration 
// as long as it draws less than power_limit. If this configuration can't be found or the pstate is invalid the function returns -1 
int profiler_isoenergy(int from_threads, int pstate, int* threads){
	
	if(/*pstate < 0 ||*/ pstate > max_pstate)
		return -1;

	double old_power = power_profile[from_threads][current_pstate];

	// There could be no number of threads > 0 such that the power consumption is less than old_power 
	
	/*if(power_profile[1][pstate] > old_power){
		if (power_profile[1][pstate] < power_limit){
			*threads = 1;
			return 0;
		}
		else return -1;
	}*/

	// If no configuration at requested pstate consumes less power than the last configuration return the configuration with 1 thread 
	if(power_profile[1][pstate] > old_power){
		*threads = 1;
		return 0;
	}

	int i = 1;
	while( i<=total_threads && power_profile[i][pstate] < old_power)
		i++;

	*threads = --i;

	#ifdef DEBUG_HEURISTICS
		printf("Isoenergy configuration: %d threads at %d pstate\n", *threads, pstate);
	#endif 

	return 0;
}  

// Returns a configuration at the given pstate with power consumption less than the one of the current configuration 
// All the power related data is retrieved from the profiler_matrix and is only used as a starting point, as this values change based on the workload
// If no configuration at the given pstate uses less than the configuration power the algorithm returns the lowest consuming configuration 
// as long as it draws less than power_limit. If this configuration can't be found or the pstate is invalid the function returns -1 
// If the power of the current is far from power_limit it directly jumps to a lower pstate such that it uses the same number of threads and consumes more than
// a user defined percentage of the power_limit	

int profiler_isoenergy_fast(int from_threads, int power, int* to_threads, int* to_pstate){

	double profiler_ratio;
	double profiler_power_limit, profiler_power_threshold;

	if(current_pstate == 0 || current_pstate > max_pstate)
		return -1;

	double profiler_power = power_profile[from_threads][current_pstate];
	profiler_ratio = profiler_power/power;
	profiler_power_limit = profiler_ratio*power_limit;
	profiler_power_threshold = (profiler_power_limit)*(( ((double) 100)-jump_percentage)/((double) 100)); 


	if(profiler_power < profiler_power_threshold){	// Should jump
		int pstate = current_pstate;
		while(pstate >= 0 && power_profile[from_threads][pstate] < profiler_power_threshold)
			pstate--;
		if(pstate == -1)
			pstate = 0;
		*to_threads = from_threads;
		*to_pstate = pstate;
		#ifdef DEBUG_HEURISTICS
			printf("Isoenergy configuration fast - Jumping to: %d threads at %d pstate\n", *to_threads, *to_pstate);
		#endif

		return 0;
	}
	else{	//Should find isoenergy configuration
		int pstate = current_pstate-1;
		*to_pstate = pstate;

		// If no configuration at requested pstate consumes less power than the last configuration return the configuration with 1 thread 
		if(power_profile[1][pstate] > profiler_power){
			*to_threads = 1;
			return 0;
		}

		int i = 1;
		while( i<=total_threads && power_profile[i][pstate] < profiler_power)
			i++;

		*to_threads = --i;
		#ifdef DEBUG_HEURISTICS
			printf("Isoenergy configuration fast - Moving to: %d threads at %d pstate\n", *to_threads, pstate);
		#endif 

		return 0;
	}

}


///////////////////////////////////////////////////////////////
// Heuristic search functions
///////////////////////////////////////////////////////////////


// HEURISTIC_MODE 0
// This heuristics converges to the configuration with the highest throughput that draws less power than power_limit 
// It starts from the bottom and finds the best configuration at each p-state that is optimal and compares it with the current best
// It needs to explore bidictionally as this makes no assumptions other that the explored surface is continous and that decreasing pstate increases power
// as well as increaasing number of active threads increaseas power
void heuristic_power(double throughput, double abort_rate, double power, double energy_per_tx){
	
	if(new_pstate){
		new_pstate = 0;
		if( power > power_limit ){
			decreasing = 1;
			if(active_threads > 1)
				pause_thread(active_threads-1);
			else stop_searching();
		}
		else{
			update_level_best_config(throughput);
			if(active_threads < total_threads){
				wake_up_thread(active_threads);
			}
			else{
				decreasing = 1;
				if(active_threads > 1)
					pause_thread(active_threads-1);
				else stop_searching();
			}
		}
		
	}
	else{	//Not new state
		
		// Increasing number of threads 
		if(!decreasing){
			if( throughput < old_throughput || power > power_limit || active_threads >= total_threads){	// Increasing should stop 
				if(power<power_limit)
					update_level_best_config(throughput);

				// If already increased once or cannot reduce wrt to starting threads there is no reason to explore descreasing threads 
				if( (active_threads - level_starting_threads) > 1 || (level_starting_threads - 1) < 1){
					new_pstate = 1;
					compare_best_level_config();
					if( current_pstate - 1 < 0)
						stop_searching();
					else{
						set_threads(level_best_threads);
						set_pstate(current_pstate - 1);
						decreasing = 0;
						level_starting_threads = level_best_threads;
						level_best_throughput = 0;
						level_best_threads = 0;
						level_best_pstate = 0;
					}
				}
				else{
					decreasing = 1;
					set_threads(level_starting_threads - 1);
				}
			}
			else{	// Should continue increasing 
				update_level_best_config(throughput);
				wake_up_thread(active_threads);
			}

		}
		// Decreasing number of threads
		else{

			if(power < power_limit)
					update_level_best_config(throughput);

			if(throughput < level_best_throughput || active_threads == 1){
				new_pstate = 1;
				compare_best_level_config();
				if( current_pstate - 1 < 0 )
					stop_searching();
				else{
					set_threads(level_best_threads);
					set_pstate(current_pstate - 1);
					decreasing = 0;
					level_starting_threads = level_best_threads;
					level_best_throughput = 0;
					level_best_threads = 0;
					level_best_pstate = 0;
				}
			}
			else pause_thread(active_threads - 1);
		
		}
	}


	//Update old global variables 
	old_throughput = throughput;
	old_abort_rate = abort_rate;
	old_power = power;
	old_energy_per_tx = energy_per_tx;
}





// HEURISTIC_MODE 1 
// This heuristic uses the power profiler to achieve a faster convergence to the optimal value.
// It finds the configuration with the highest throughput that drawns less power than power_limit defined in hope_config file 
void heuristic_power_profiler(double throughput, double abort_rate, double power, double energy_per_tx){
			
	if(new_pstate){
		new_pstate = 0;
		if(power > power_limit){	// Power is higher than limit
			decreasing = 1;
			if(active_threads > 1){
				pause_thread(active_threads-1);
			}
			else 
				stop_searching();
		}else{	// Power lower than limit
			decreasing = 0;
			update_best_config(throughput, power);

			if(active_threads < total_threads)
				wake_up_thread(active_threads);
			else{
				int threads;
				if(profiler_isoenergy(active_threads, current_pstate-1, &threads) != -1){
					set_pstate(current_pstate-1);
					set_threads(threads);
					new_pstate = 1;
				}
				else stop_searching();
			}
		}
	}
	else{ // Not new_pstate, should check if decreasing or not
		if(decreasing){
			if(power < power_limit){
				update_best_config(throughput, power);
				decreasing = 0;
				int threads;
				if(profiler_isoenergy(active_threads, current_pstate-1, &threads) != -1){
					set_pstate(current_pstate-1);
					set_threads(threads);
					new_pstate = 1;
				}
				else stop_searching();
			}
			else{
				if(active_threads >1)
					pause_thread(active_threads-1);
				else stop_searching();
			}
		}
		else{	// Increasing number of threads 
			int improved = 0;
			if(throughput > old_throughput && power < power_limit)
				improved = 1;

			if(improved){
				update_best_config(throughput, power);
				if(active_threads < total_threads){
					wake_up_thread(active_threads);
				}
				else{
					int threads;
					if(profiler_isoenergy(active_threads, current_pstate-1, &threads) != -1){
						set_pstate(current_pstate-1);
						set_threads(threads);
						new_pstate = 1;
					}
					else stop_searching();
				}
			}
			else{ // not improved
				int threads;
					if(profiler_isoenergy(active_threads-1, current_pstate-1, &threads) != -1){
						set_pstate(current_pstate-1);
						set_threads(threads);
						new_pstate = 1;
					}
					else stop_searching();
			}

		}	
	}

	//Update old global variables 
	old_throughput = throughput;
	old_abort_rate = abort_rate;
	old_power = power;
	old_energy_per_tx = energy_per_tx;
}

// HEURISTIC_MODE 2
// This heuristic uses the power profiler to achieve a faster convergence to the optimal value. It jumps directly to the lowest pstate configuration such that:
// it has the same number of threads it consumes less than a defined percentage of power limit. This percentage is defined in hope_config

void heuristic_power_profiler_fast(double throughput, double abort_rate, double power, double energy_per_tx){
	
	if(new_pstate){
		new_pstate = 0;
		if(power > power_limit){	// Power is higher than limit
			decreasing = 1;
			if(active_threads > 1){
				pause_thread(active_threads-1);
			}
			else 
				stop_searching();
		}else{	// Power lower than limit
			decreasing = 0;
			update_best_config(throughput, power);

			if(active_threads < total_threads)
				wake_up_thread(active_threads);
			else{
				int threads;
				int to_pstate;
				if(profiler_isoenergy_fast(active_threads, power, &threads, &to_pstate) != -1){
					set_pstate(to_pstate);
					set_threads(threads);
					new_pstate = 1;
				}
				else stop_searching();
			}
		}
	}
	else{ // Not new_pstate, should check if decreasing or not
		if(decreasing){
			if(power < power_limit){
				update_best_config(throughput, power);
				decreasing = 0;
				int threads;
				int to_pstate;
				if(profiler_isoenergy_fast(active_threads, power, &threads, &to_pstate) != -1){
					set_pstate(to_pstate);
					set_threads(threads);
					new_pstate = 1;
				}
				else stop_searching();
			}
			else{
				if(active_threads >1)
					pause_thread(active_threads-1);
				else stop_searching();
			}
		}
		else{	// Increasing number of threads 
			int improved = 0;
			if(throughput > old_throughput && power < power_limit)
				improved = 1;

			if(improved){
				update_best_config(throughput, power);
				if(active_threads < total_threads){
					wake_up_thread(active_threads);
				}
				else{
					int threads;
					int to_pstate;
					if(profiler_isoenergy_fast(active_threads, power, &threads, &to_pstate) != -1){
						set_pstate(to_pstate);
						set_threads(threads);
						new_pstate = 1;
					}
					else stop_searching();
				}
			}
			else{ // not improved
				int threads;
				int to_pstate;
					if(profiler_isoenergy_fast(active_threads-1, power, &threads, &to_pstate) != -1){
						set_pstate(to_pstate);
						set_threads(threads);
						new_pstate = 1;
				}
					else stop_searching();
			}

		}	
	}

	//Update old global variables 
	old_throughput = throughput;
	old_abort_rate = abort_rate;
	old_power = power;
	old_energy_per_tx = energy_per_tx;
}

// HEURISTIC_MODE 3. This heuristics converges to the configuration with the highest throughput that consumes less than energy_per_tx on average for transaction
// Hypothesis no local max on throughput. 
void heuristic_energy_bidirectional(double throughput, double abort_rate, double power, double energy_per_tx){

	if(new_pstate){
		new_pstate = 0;
		level_starting_energy_per_tx = energy_per_tx;
		
		if(energy_per_tx < energy_per_tx_limit)
			update_level_best_config(throughput);			

		if(active_threads < total_threads){
			decreasing = 0;
			wake_up_thread(active_threads);
		}
		else{
			if(active_threads != 1 && (level_starting_threads-1) > 0) {
				decreasing = 1;
				pause_thread(active_threads-1);
			}
			else{
				new_pstate = 1;
				decreasing = 0;
				compare_best_level_config();
				if(current_pstate == 0 ){
					stop_searching();
				}
				else{
					if(level_best_threads != 0)
						level_starting_threads = level_best_threads;
					level_best_threads = 0;
					level_best_throughput = 0;
					level_best_pstate = 0;
					set_threads(level_starting_threads);
					set_pstate(current_pstate-1);
				}
			}
		}
	}
	else{
		if(!decreasing){	// Increasing
			if(energy_per_tx < energy_per_tx_limit){
				update_level_best_config(throughput);
				if(active_threads == total_threads || old_throughput > throughput){ // Switch to decreasing if possible or move to next p-state
					if(active_threads != 1 && (level_starting_threads-1) > 0) {
						decreasing = 1;
						pause_thread(active_threads-1);
					}
					else{
						new_pstate = 1;
						decreasing = 0;
						compare_best_level_config();
						if(current_pstate == 0 ){
							stop_searching();
						}
						else{
							if(level_best_threads != 0)
								level_starting_threads = level_best_threads;
							level_best_threads = 0;
							level_best_throughput = 0;
							level_best_pstate = 0;
							set_threads(level_starting_threads);
							set_pstate(current_pstate-1);
						}
					}
				}
				else wake_up_thread(active_threads);
			}
			else{	// Energy higher than limit 
				if(energy_per_tx > old_energy_per_tx || active_threads == total_threads){	// Switch to decreasing if possible or move to next p-state
					if(active_threads != 1 && (level_starting_threads-1) > 0) {
						decreasing = 1;
						pause_thread(active_threads-1);
					}
					else{
						new_pstate = 1;
						decreasing = 0;
						compare_best_level_config();
						if(current_pstate == 0 ){
							stop_searching();
						}
						else{
							if(level_best_threads != 0)
								level_starting_threads = level_best_threads;
							level_best_threads = 0;
							level_best_throughput = 0;
							level_best_pstate = 0;
							set_threads(level_starting_threads);
							set_pstate(current_pstate-1);
						}
					}
				}
				else wake_up_thread(active_threads);
			}
		}
		else{	// Decreasing
			if(energy_per_tx < energy_per_tx_limit){
				update_level_best_config(throughput);
				if(active_threads == 1 || level_best_throughput > throughput){
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0){
						stop_searching();
					}	
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
				else pause_thread(active_threads-1);
			}
			else{	// Energy higher than limit 
				if(active_threads == (level_starting_threads-1))	// In the first decreasing step last energy is related to the increasing phase
					old_energy_per_tx = level_starting_energy_per_tx;
				if(energy_per_tx > old_energy_per_tx || active_threads == 1){
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0){
						stop_searching();
					}	
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
				else pause_thread(active_threads-1);
			}
		}
	}

	//Update old global variables 
	old_throughput = throughput;
	old_abort_rate = abort_rate;
	old_power = power;
	old_energy_per_tx = energy_per_tx;
}


// HEURISTIC_MODE 4. This heuristics converges to the configuration with the highest throughput that consumes less than energy_per_tx on average for transaction
// Hypothesis no local max on throughput. Increasing p-state the optimal configuration will move towards using less threads.
// Given the optimal amount of threads t for a pstate p, if we consider a pstate<p the optimal amount of threads will be <=t
void heuristic_energy_unidirectional(double throughput, double abort_rate, double power, double energy_per_tx){

	if(new_pstate){
		new_pstate = 0;
		level_starting_energy_per_tx = energy_per_tx;

		if(energy_per_tx < energy_per_tx_limit)
			update_level_best_config(throughput);			


		if(current_pstate == max_pstate){	// Try to increase and then decrease	
			if(active_threads < total_threads){
				decreasing = 0;
				wake_up_thread(active_threads);
			}
			else{
				if(active_threads != 1 && (level_starting_threads-1) > 0) {
					decreasing = 1;
					pause_thread(active_threads-1);
				}
				else{
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0 ){
						stop_searching();
					}
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
			}
		}else{	// With pstate > max_pstate only decrease the number of threads
			if(active_threads != 1 && (level_starting_threads-1) > 0) {
					decreasing = 1;
					pause_thread(active_threads-1);
				}
				else{
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0 ){
						stop_searching();
					}
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
		}
		
	}
	else{
		if(!decreasing){	// Increasing
			if(energy_per_tx < energy_per_tx_limit){
				update_level_best_config(throughput);
				if(active_threads == total_threads || old_throughput > throughput){ // Switch to decreasing if possible or move to next p-state
					if(active_threads != 1 && (level_starting_threads-1) > 0) {
						decreasing = 1;
						pause_thread(active_threads-1);
					}
					else{
						new_pstate = 1;
						decreasing = 0;
						compare_best_level_config();
						if(current_pstate == 0 ){
							stop_searching();
						}
						else{
							if(level_best_threads != 0)
								level_starting_threads = level_best_threads;
							level_best_threads = 0;
							level_best_throughput = 0;
							level_best_pstate = 0;
							set_threads(level_starting_threads);
							set_pstate(current_pstate-1);
						}
					}
				}
				else wake_up_thread(active_threads);
			}
			else{	// Energy higher than limit 
				if(energy_per_tx > old_energy_per_tx || active_threads == total_threads){	// Switch to decreasing if possible or move to next p-state
					if(active_threads != 1 && (level_starting_threads-1) > 0) {
						decreasing = 1;
						pause_thread(active_threads-1);
					}
					else{
						new_pstate = 1;
						decreasing = 0;
						compare_best_level_config();
						if(current_pstate == 0 ){
							stop_searching();
						}
						else{
							if(level_best_threads != 0)
								level_starting_threads = level_best_threads;
							level_best_threads = 0;
							level_best_throughput = 0;
							level_best_pstate = 0;
							set_threads(level_starting_threads);
							set_pstate(current_pstate-1);
						}
					}
				}
				else wake_up_thread(active_threads);
			}
		}
		else{	// Decreasing
			if(energy_per_tx < energy_per_tx_limit){
				update_level_best_config(throughput);
				if(active_threads == 1 || level_best_throughput > throughput){
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0){
						stop_searching();
					}	
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
				else pause_thread(active_threads-1);
			}
			else{	// Energy higher than limit 
				if(active_threads == (level_starting_threads-1))	// In the first decreasing step last energy is related to the increasing phase
					old_energy_per_tx = level_starting_energy_per_tx;
				if(energy_per_tx > old_energy_per_tx || active_threads == 1){
					new_pstate = 1;
					decreasing = 0;
					compare_best_level_config();
					if(current_pstate == 0){
						stop_searching();
					}	
					else{
						if(level_best_threads != 0)
							level_starting_threads = level_best_threads;
						level_best_threads = 0;
						level_best_throughput = 0;
						level_best_pstate = 0;
						set_threads(level_starting_threads);
						set_pstate(current_pstate-1);
					}
				}
				else pause_thread(active_threads-1);
			}
		}
	}

	//Update old global variables 
	old_throughput = throughput;
	old_abort_rate = abort_rate;
	old_power = power;
	// Used for testing all possible configurations pstate/threads
	old_energy_per_tx = energy_per_tx;
}


void explore_all_configurations(double throughput, double abort_rate, double power, double energy_per_tx){

	if(active_threads == total_threads || abort_rate>90.0){
		if( current_pstate > 0 ){
			set_pstate(current_pstate-1);
			set_threads(1);
			printf("\n");
		}
	}
	else set_threads(active_threads+1);
}


// Helper function for dynamic heuristic0, called in phase 0
void from_phase0_to_next(){
	
	if(best_throughput > 0){
		phase0_threads = best_threads;
		phase0_pstate = current_pstate;
	}
	else{
		phase0_threads = 1;
		phase0_pstate = current_pstate;
	}

	if(current_pstate == 0){
		if(best_threads == total_threads){
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> END\n");
			#endif
			stop_searching();
		}
		else{
			phase = 2; 

			if(best_throughput > 0){
				set_threads(best_threads);
				set_pstate(current_pstate+1);
			}

			level_best_throughput = 0;
			level_best_threads = 0;
			level_best_pstate = 0;
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> PHASE 2\n");
			#endif
		}
	}else{
		phase = 1;
		if(best_throughput > 0){
			set_threads(best_threads);
			set_pstate(current_pstate-1);
		}
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 0 -> PHASE 1\n");
		#endif
	}
}

// Helper function for dynamic heuristic0, called in phase 1
void from_phase1_to_next(){
	// Check if should move to phase 0 or should stop searching 
	if(phase0_pstate == max_pstate || best_threads == total_threads || phase0_threads == total_threads){
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 -> END\n");
		#endif
		stop_searching();
	}
	else{
		phase = 2;
		if(phase0_threads > 0){
			set_threads(phase0_threads);
			set_pstate(phase0_pstate+1);
		}
		level_best_threads = 0;
		level_best_throughput = 0;
		level_best_pstate = 0;
		#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 -> PHASE 2\n");
		#endif
	}
}


// Baseline dynamic exploration. In phase 0 the explorations looks for the number of threads that provides the best performance. 
// In phase 1 the algorithm iteratively decreases the p-state and seeks for the first config within the powercap at each frequency/voltage.
// The exploration at the next p_state always starts from the number of threads found optimal from the previous performance state and only decreases it if needed 
void dynamic_heuristic0(double throughput, double abort_rate, double power, double energy_per_tx){
	
	if(phase == 0){	// Thread scheduling at lowest frequency
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if(steps == 0){ // First exploration step
			if(active_threads != total_threads && power < power_limit){
				set_threads(active_threads+1);
			}
			else if(active_threads > 1){
				decreasing = 1;
				set_threads(active_threads-1);
				
				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 - DECREASING\n");
				#endif
			}
			else{ //Starting from 1 thread and cannote increase 
				from_phase0_to_next();
			}
		}
		else if(steps == 1 && !decreasing){ //Second exploration step, define if should set decreasing 
			if(throughput >= best_throughput*0.9 && power < power_limit && active_threads != total_threads){
				set_threads(active_threads+1);
			} else{ // Should set decreasing to 0 
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next();
				}
			}
		} 
		else if(decreasing){ // Decreasing threads
			if(throughput < best_throughput*0.9 || active_threads == 1)
				from_phase0_to_next();
			else
				set_threads(active_threads-1);
			
		} else{ // Increasing threads
			if( power > power_limit || active_threads == total_threads || throughput < best_throughput*0.9){
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next();
				}
			}
			else set_threads(active_threads+1);
		}
	}
	else if(phase == 1){ //Increase in performance states while being within the power cap
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (power < power_limit && current_pstate == 0) || (power > power_limit && active_threads == 1) )
			from_phase1_to_next();
		
		else{ // Not yet completed to explore in phase 1 
			if(power < power_limit) //Should decrease number of active threads
				set_pstate(current_pstate-1);
			else // Power beyond power limit
				set_threads(active_threads-1);
		}	
	}
	else{ // Phase == 2. Decreasing the CPU frequency and increasing the number of threads. Not called in the first exploration phase 
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (current_pstate == max_pstate && power>power_limit) || (current_pstate == max_pstate && active_threads == total_threads) || throughput < level_best_throughput || (active_threads == total_threads && power<power_limit ))
			stop_searching();
		else{ // Should still explore in phase 2 
			if(power < power_limit){
				update_level_best_config(throughput);
				set_threads(active_threads+1);
			}
			else{ // Outside power_limit, should decrease P-state and reset level related data
				set_pstate(current_pstate+1);
				level_best_threads = 0;
				level_best_throughput = 0;
				level_best_pstate = 0;
			}
		}
	}
}

// Utility function used for updating the values of the HIGH configurations
void update_high(double throughput, double power){

	if(power < power_limit*(1+(extra_range_percentage/100)) && throughput > high_throughput){
		high_throughput = throughput; 
		high_pstate = current_pstate;
		high_threads = active_threads;
		high_power = power;
	} 
}

// Utility function used for updating the values of the HIGH configurations
void update_low(double throughput, double power){

	if( power < power_limit*(1-(extra_range_percentage/100/2)) && throughput > low_throughput){
		low_throughput = throughput; 
		low_pstate = current_pstate;
		low_threads = active_threads; 
		low_power = power;
	}
}

// Evolution of heuristic0 that saves 3 distinct configurations to be alternated in the exploitation phase.
// This alternation allows to simulate a virtual configuration that has a power consumption as close as possible to the cap and increases the capability of adapting during noisy explorations 
void dynamic_heuristic1(double throughput, double abort_rate, double power, double energy_per_tx){

	update_high(throughput, power); 
	update_low(throughput, power); 

	// Call basic dynamic heuristic
	dynamic_heuristic0(throughput, abort_rate, power, energy_per_tx);
}


void update_highest_threads(double throughput, double power){
	
	if( power < power_limit){
		if(active_threads == best_threads){
			if(best_pstate == -1 || current_pstate < best_pstate){
				best_throughput = throughput;
				best_threads = active_threads;
				best_pstate = current_pstate;
			}
		}
		else if( active_threads > best_threads){
			best_throughput = throughput;
			best_threads = active_threads;
			best_pstate = current_pstate;
		}					
	}
}

// Heuristic similar to to Cap and Pack. Used as a comparison. Consider the best config the one with the most active threads. Within the same number of threads the one within the cap with the highest frequency is selected 
void heuristic_highest_threads(double throughput, double abort_rate, double power, double energy_per_tx){
	
	update_highest_threads(throughput, power);

	if(phase == 0){	// Find the highest number of threads at the lowest P-state
		if(steps == 0){
			if(active_threads == total_threads || power > power_limit){
				decreasing = 1; 
				set_threads(active_threads-1);
			}
			else 
				set_threads(active_threads+1);
		}else{
			if(decreasing){
				if(power < power_limit){
					if(best_pstate != 0){
						phase = 1; 
						set_threads(best_threads);
						set_pstate(best_pstate-1);
					}else stop_searching();
				} else set_threads(active_threads-1);
			}else{	//Increasing
				if( power > power_limit || active_threads == total_threads){
					phase = 1;
					set_threads(best_threads);
					set_pstate(best_pstate-1);
				} else set_threads(active_threads+1);
			}
		}
	}
	else if (phase == 1){
		if(power > power_limit || current_pstate == 0 )
			stop_searching();
		else set_pstate(current_pstate-1);
	}
}
	

// Explore the number active threads and DVFS settings indepedently. Implemented as a comparison to a state-of-the-art solution that considers the different power management knobs independently which might be sub-optimal. 
// When this policy is set, the exploration starts with 1 thread at the maximum p-state
void heuristic_binary_search(double throughput, double abort_rate, double power, double energy_per_tx){

	if(phase == 0){ // Thread tuning

		// First two steps should check performance results with lowest number of active threads (1) and highest. If the latter performs worse than then former should directly move to DVFS tuning
		if(steps == 0){
			min_thread_search_throughput = throughput;
			set_threads(total_threads);
			steps++;
		}else if(steps == 1){
			max_thread_search_throughput = throughput;
			if(max_thread_search_throughput < min_thread_search_throughput){
				phase = 1;
				set_threads(1);
				set_pstate((int) max_pstate/2);

				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 --> PHASE 1\n");
				#endif

			}else set_threads(min_thread_search+((int) (max_thread_search - min_thread_search) /2));
		}else{ 	
			if(min_thread_search >= max_thread_search){ // Stop the binary search on threads and move on dvfs
				phase = 1; 
				set_pstate((int) max_pstate/2);

				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 --> PHASE 1\n");
				#endif

			}else{ // Keep searching
				if(power > power_limit || throughput > max_thread_search_throughput){ // Should set current to high
					max_thread_search = active_threads;
					max_thread_search_throughput = throughput; 
				}else{ // Should set current to low 
					min_thread_search = active_threads;
					min_thread_search_throughput = throughput; 
				}
				set_threads(min_thread_search+((int) (max_thread_search - min_thread_search) /2));
			}
		}
		
		#ifdef DEBUG_HEURISTICS
			printf("SEARCH RANGE - THREADS: %d - %d \n", min_thread_search, max_thread_search);
		#endif

	}else{ // DVFS tuning, phase == 1 

		if(min_pstate_search >= max_pstate_search){
			
			#ifdef DEBUG_HEURISTICS
					printf("PHASE 1 --> END\n");
			#endif


			update_best_config(throughput, power);
			stop_searching();
		}else{ 	// Decreasing the p-state always improves performance
			if(power < power_limit) 
				max_pstate_search = current_pstate;
			else min_pstate_search = current_pstate;

			set_pstate(min_pstate_search+( (int) ceil(((double) max_pstate_search - (double) min_pstate_search)/2)));
		}

		#ifdef DEBUG_HEURISTICS
			printf("SEARCH RANGE - P-STATE: %d - %d \n", min_pstate_search, max_pstate_search);
		#endif
	}
}

// In phase 1 this heuristic first explores in the direction of increased active threads then,
// if either performance decreases or the power consumption reaches the power limit, it to phase 2 where the DVFS setting, for that given amount of active threads, is tuned
void heuristic_two_step_search(double throughput, double abort_rate, double power, double energy_per_tx){

	if(power<power_limit){
		update_best_config(throughput,power);
	}

	if(phase == 0){ // Searching threads
		if(power<power_limit && active_threads < total_threads && throughput > best_throughput*0.9){
			set_threads(active_threads+1);
		}else{
			if(best_throughput != -1){
				set_threads(best_threads);
			}
			set_pstate(current_pstate-1);
			phase = 1; 
		}
	}else{ // Phase == 1. Optimizing DVFS
		if(power<power_limit && current_pstate > 0){
			set_pstate(current_pstate-1);
		}
		else{
			stop_searching();
		}
	}
}


// Helper function for dynamic heuristic0, called in phase 0
void from_phase0_to_next_stateful(){

	phase == 1;

	if(best_throughput == -1){
		set_pstate(current_pstate+1);
	}else{

		if(current_pstate == 0)
			stop_searching();
		else{
			set_threads(best_threads);
			set_pstate(current_pstate-1);
		}
	} 
}

// Equivalent to heuristic_two_step_search but when the exploration is restarted it starts from the previous best configuration instead of 1 Thread at lowest DVFS setting. 
void heuristic_two_step_stateful(double throughput, double abort_rate, double power, double energy_per_tx){
	
	if(phase == 0){	// Thread scheduling at lowest frequency
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if(steps == 0){ // First exploration step
			if(active_threads != total_threads && power < power_limit){
				set_threads(active_threads+1);
			}
			else if(active_threads > 1){
				decreasing = 1;
				set_threads(active_threads-1);
				
				#ifdef DEBUG_HEURISTICS
					printf("PHASE 0 - DECREASING\n");
				#endif
			}
			else{ //Starting from 1 thread and cannote increase 
				from_phase0_to_next_stateful();
			}
		}
		else if(steps == 1 && !decreasing){ //Second exploration step, define if should set decreasing 
			if(throughput >= best_throughput*0.9 && power < power_limit && active_threads != total_threads){
				set_threads(active_threads+1);
			} else{ // Should set decreasing to 0 
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next_stateful();
				}
			}
		} 
		else if(decreasing){ // Decreasing threads
			if(throughput < best_throughput*0.9 || active_threads == 1)
				from_phase0_to_next_stateful();
			else
				set_threads(active_threads-1);
			
		} else{ // Increasing threads
			if( power > power_limit || active_threads == total_threads || throughput < best_throughput*0.9){
				if(starting_threads > 1){
					decreasing = 1; 
					set_threads(starting_threads-1);	

					#ifdef DEBUG_HEURISTICS
						printf("PHASE 0 - DECREASING\n");
					#endif
				}
				else{ // Cannot reduce number of thread more as starting_thread is already set to 1 
					from_phase0_to_next_stateful();
				}
			}
			else set_threads(active_threads+1);
		}
	}
	else{ // Phase == 1 -> Increase DVFS until reaching the powercap
		
		if(power<power_limit){
			update_best_config(throughput, power);
		}

		if( (power < power_limit && current_pstate == 0) || (power > power_limit && active_threads == 1) ){
		
			#ifdef DEBUG_HEURISTICS
				printf("PHASE 1 - END\n");
			#endif
			
			stop_searching();;
		} else{ // Not yet completed to explore in phase 1 
			if(power < power_limit)
				set_pstate(current_pstate-1);
			else{

				if(best_throughput == -1)
					set_pstate(current_pstate+1);
				else{
					#ifdef DEBUG_HEURISTICS
						printf("PHASE 1 - END\n");
					#endif

					stop_searching();
				}
				
			} 
		}	
	}
}

void compute_power_model(){

	double alfa, beta, pwr_h, pwr_l, freq_h, freq_l, freq_i, freq3_h, freq3_l;
	int i,j;

	freq_h = ((double) pstate[lower_sampled_model_pstate])/1000;
	freq_l = ((double) pstate[max_pstate])/1000;

	freq3_h = freq_h*freq_h*freq_h;
	freq3_l = freq_l*freq_l*freq_l;

	// Must compute specific model instance for each number of active threads
	for(j = 1; j <= total_threads; j++){
		
		pwr_h = power_model[lower_sampled_model_pstate][j] - power_uncore;
		pwr_l = power_model[max_pstate][j] - power_uncore;
		
		alfa = (pwr_l*freq_h - pwr_h*freq_l)/(freq_h*freq3_l - freq3_h*freq_l);
		beta = (pwr_l*freq_h*freq_h*freq_h - pwr_h*freq_l*freq_l*freq_l)/(freq_h*freq_h*freq_h*freq_l - freq_h*freq_l*freq_l*freq_l);

		#ifdef DEBUG_HEURISTICS
			printf("Setting up the power model ...\n");
			printf("Threads = %d - alfa = %lf - beta = %lf - power uncore = %lf - pwr_h = %lf - pwr_l = %lf - freq_h %lf - freq3_h %lf\n", 
				j, alfa, beta, power_uncore, pwr_h, pwr_l, freq_h, freq3_h);
		#endif
		
		for(i = 1; i < max_pstate; i++) {
			if (i==lower_sampled_model_pstate) continue;
			freq_i = ((double) pstate[i])/1000;
			power_model[i][j] = alfa*freq_i*freq_i*freq_i+beta*freq_i+power_uncore; 
		}
		
	}

	#ifdef DEBUG_HEURISTICS
		for(i = 1; i <= max_pstate; i++){
			for (j = 1; j <= total_threads; j++){
				printf("%lf\t", power_model[i][j]);
			}
			printf("\n");
		}
		printf("Setup of the power model completed\n");
	#endif
}

void compute_throughput_model(){

	double c,m,speedup; 
	int i,j;

	// Must compute specific model instance for each number of active threads
	for(j = 1; j <= total_threads; j++){
		speedup = throughput_model[lower_sampled_model_pstate][j]/throughput_model[max_pstate][j];
		c = (pstate[lower_sampled_model_pstate]*(1-speedup))/(speedup*(pstate[max_pstate]-pstate[lower_sampled_model_pstate]));
		m = 1-c;

		#ifdef DEBUG_HEURISTICS
			printf("Setting up the throughput model ...\n");
			printf("Threads = %d - C = %lf - M = %lf - speedup = %lf\n", j, c, m, speedup);
		#endif
		
		for(i = 1; i < max_pstate; i++) {
			if (i==lower_sampled_model_pstate) continue;
			throughput_model[i][j] = (1/(((double) pstate[max_pstate] * 1000)/((double) pstate[i] * 1000)*c+m))*throughput_model[max_pstate][j];
		}
		
	}

	#ifdef DEBUG_HEURISTICS
		for(i = 1; i <= max_pstate; i++){
			for (j = 1; j <= total_threads; j++){
				printf("%lf\t", throughput_model[i][j]);
			}
			printf("\n");
		}
		printf("Setup of the throughput model completed\n");
	#endif
}


// Relies on power and performance models to predict power and performance.The setup of the models
// require to sample power and performance of all configurations with P-state = 1 and P-state = max_pstate.
// In the initial phase, the setup is performed. Consequently, the models are used to selects the best configuration
// under the power cap based on their predictions. 
void model_power_throughput(double throughput, double power){

 int i, j;

	power_model[current_pstate][active_threads] = power;
	throughput_model[current_pstate][active_threads] = throughput;

	if(current_pstate == max_pstate){
		power_real[current_pstate][active_threads] = power;
		throughput_real[current_pstate][active_threads] = throughput;
		power_validation[current_pstate][active_threads] = power_model[current_pstate][active_threads];
		throughput_validation[current_pstate][active_threads] = throughput_model[current_pstate][active_threads];
	}

	if(active_threads == total_threads && current_pstate == lower_sampled_model_pstate){
		
		compute_power_model();
		compute_throughput_model();

		// Init best_threads and best_pstate
		best_threads = 1;
		best_pstate = max_pstate;
		best_throughput = throughput_model[max_pstate][1];

		for(i = 1; i <= max_pstate; i++){
			for(j = 1; j <= total_threads; j++){
				if(power_model[i][j] < power_limit && throughput_model[i][j] > best_throughput){
					best_pstate = i;
					best_threads = j;
					best_throughput = throughput_model[i][j];
				}
			}
		}
		
		stop_searching();

	}else{ 

		if(active_threads < total_threads)
			set_threads(active_threads+1);
		else{
			set_pstate(lower_sampled_model_pstate);
			set_threads(1);
		}
	} 
}

char* subString (const char* input, int offset, int len, char* dest){
	int input_len = strlen (input);
	if (offset + len > input_len){
     		return NULL;
  	}
	strncpy(dest, input + offset, len);
  	return dest;
}

void baseline_enhanced(double throughput, double abort_rate, double power, double energy_per_tx){
	heuristic_highest_threads(throughput, abort_rate, power, energy_per_tx);
}


///////////////////////////////////////////////////////////////
// Main heuristic function
///////////////////////////////////////////////////////////////


// Takes decision on frequency and number of active threads based on statistics of current round 
	void heuristic(double throughput, double abort_rate, double power, double energy_per_tx, long time){
		
		#ifdef DEBUG_HEURISTICS 
			printf("Throughput: %lf - Abort rate: %lf percent - Power: %lf Watt - Time: %lf microseconds\n",
			        throughput, abort_rate, power, ((double) time/1000000));
		#endif

		#ifdef TIMELINE_PLOT
			double current_execution_time_ms = ( (double) (get_time() - time_application_startup))/1000000;

			// Line Semantics -> Time(milliseconds), Power (Watt), Powercap (Watt), Throughput, P-state, threads 
			fprintf(timeline_plot_file, "%lf,%lf,%lf,%lf,%d,%d\n", current_execution_time_ms, power, power_limit, throughput, current_pstate, active_threads);
		#endif

		#ifdef DEBUG_OVERHEAD
			long time_heuristic_start;
			long time_heuristic_end;
			double time_heuristic_microseconds;

			time_heuristic_start = get_time();
		#endif 

		if(!stopped_searching){
			switch(heuristic_mode){
				case 0:
					heuristic_power(throughput, abort_rate, power, energy_per_tx);
					break;
				case 1:
					heuristic_power_profiler(throughput, abort_rate, power, energy_per_tx);
					break;
				case 2:
					heuristic_power_profiler_fast(throughput, abort_rate, power, energy_per_tx);
					break;
				case 3:
					heuristic_energy_bidirectional(throughput, abort_rate, power, energy_per_tx);
					break;
				case 4:
					heuristic_energy_unidirectional(throughput, abort_rate, power, energy_per_tx);
					break;	
				case 5:
					explore_all_configurations(throughput, abort_rate, power, energy_per_tx);
					break;
				case 6:	// Static max frequency, number of threads defined by file 
					set_pstate(0);
					best_pstate = current_pstate;
					best_threads = active_threads;
					stop_searching();
					break;
				case 7: // Only thread scheduling at P-state 0
					set_pstate(0);
					heuristic_mode = 0;
					heuristic_power(throughput, abort_rate, power, energy_per_tx);
					break;
				case 8: // Fixed number of threads at p_state static_pstate set in hope_config.txt
					stopped_searching = 1;
					break;
				case 9:	// Dynamic heuristic0
					dynamic_heuristic0(throughput, abort_rate, power, energy_per_tx);
					break;
				case 10: 
					dynamic_heuristic1(throughput, abort_rate, power, energy_per_tx);
					break;
				case 11:
					heuristic_highest_threads(throughput, abort_rate, power, energy_per_tx);
					break;
				case 12:
					heuristic_binary_search(throughput, abort_rate, power, energy_per_tx);
					break;
				case 13:
					heuristic_two_step_search(throughput, abort_rate, power, energy_per_tx);
					break;
				case 14:
					heuristic_two_step_stateful(throughput, abort_rate, power, energy_per_tx);
					break;
				case 15:
					model_power_throughput(throughput, power);
					break;
				case 16:
					baseline_enhanced(throughput, abort_rate, power, energy_per_tx);
					break;
			}

			if(!stopped_searching)
				steps++;

			#ifdef DEBUG_HEURISTICS
				printf("Switched to: #threads %d - pstate %d\n", active_threads, current_pstate);
			#endif 
		}
		else{	// Workload change detection
			if(detection_mode == 1){
				if( throughput > (best_throughput*(1+(detection_tp_threshold/100))) || throughput < (best_throughput*(1-(detection_tp_threshold/100))) || power > (power_limit*(1+(detection_pwr_threshold/100))) || power < (power_limit*(1-(detection_pwr_threshold/100))) ){
					stopped_searching = 0;
					set_pstate(max_pstate);
					set_threads(starting_threads);
					best_throughput = -1;
					best_threads = starting_threads;
					best_pstate = max_pstate;
					printf("Workload change detected. Restarting heuristic search\n");
				}		
			}
			else if(detection_mode == 3){
				if(heuristic_mode == 15){
				
				// Copy sample data to compare models with real data
				power_real[current_pstate][active_threads] = power;
				throughput_real[current_pstate][active_threads] = throughput;

				// Copy to the validation array predictions from the model. Necessary as we perform multiple runs of the model
				// to account for workload variability
				power_validation[current_pstate][active_threads] = power_model[current_pstate][active_threads];
				throughput_validation[current_pstate][active_threads] = throughput_model[current_pstate][active_threads];
				
				if(current_pstate == 1 && active_threads == total_threads){

					// Set validation for p-state equal to lower_sampled_model_pstate
					for(int l = 1; l<total_threads; l++){
						power_real[lower_sampled_model_pstate][l] = power_model[lower_sampled_model_pstate][l];
						throughput_real[lower_sampled_model_pstate][l] = throughput_model[lower_sampled_model_pstate][l];
						power_validation[lower_sampled_model_pstate][l] = power_model[lower_sampled_model_pstate][l];
						throughput_validation[lower_sampled_model_pstate][l] = throughput_model[lower_sampled_model_pstate][l];
					}
				
						
					// Do not restart the exploration/model setup
					detection_mode = 0;
					// Print validation results to file

					extern char *__progname;
					char output_filename[64];

					sprintf(output_filename, "%s-model_validation.txt", __progname);
	
					printf ("\nWriting model validation data to file: %s", output_filename);
					fflush(stdout);

					FILE* model_validation_file = fopen(output_filename,"w+");
					int i,j, total_confgurations_for_thr_mre=0, total_confgurations_for_pow_mre = 0;
					double throughput_re, throughput_abs_re_sum=0;
					double power_re, power_abs_re_sum=0;
					// Write real, predicted and error for throughput to file
					fprintf(model_validation_file, "Real throughput\n");
					for(i = 1; i <= max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", throughput_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Predicted throughput\n");
					for(i = 1; i <= max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", throughput_validation[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Throughput error percentage\n");
					for(i = 1; i <= max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							throughput_re=(throughput_validation[i][j]-throughput_real[i][j])/throughput_real[i][j];
							fprintf(model_validation_file, "%lf\t", 100*throughput_re);
							if (i!=lower_sampled_model_pstate && i!=max_pstate) {
								throughput_abs_re_sum+=fabs(throughput_re);
								total_confgurations_for_thr_mre++;							
							} 
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					// Write real, predicted and error for power to file
					fprintf(model_validation_file, "Real power\n");
					for(i = 1; i <= max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", power_real[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "Predicted power\n");
					for(i = 1; i <= max_pstate; i++){
						for (j = 1; j <= total_threads; j++){
							fprintf(model_validation_file, "%lf\t", power_validation[i][j]);
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");

					fprintf(model_validation_file, "power error percentage\n");
					for(i = 1; i <= max_pstate; i++){						
						for (j = 1; j <= total_threads; j++){
							power_re=(power_validation[i][j]-power_real[i][j])/power_real[i][j];
							fprintf(model_validation_file, "%lf\t", 100*power_re);
							if (i!=lower_sampled_model_pstate && i!=max_pstate) {
								power_abs_re_sum+=fabs(power_re);
								total_confgurations_for_pow_mre++;
							}
						}
						fprintf(model_validation_file, "\n");
					}
					fprintf(model_validation_file, "\n");
					fclose(model_validation_file);

					sprintf(output_filename, "%s-throughput_percent_mre.txt", __progname);
					printf ("\nWriting model validation data to file: %s", output_filename);
					fflush(stdout);
					FILE* model_percent_mre = fopen(output_filename,"a");
						fprintf(model_percent_mre, "%lf\n", throughput_abs_re_sum/(double) total_confgurations_for_thr_mre*(double)100);
					fclose(model_percent_mre);

					sprintf(output_filename, "%s-power_percent_mre.txt", __progname);
					printf ("\nWriting model validation data to file: %s", output_filename);
					fflush(stdout);
					FILE* power_percent_mre = fopen(output_filename,"a");
						fprintf(power_percent_mre, "%lf\n", power_abs_re_sum/(double) total_confgurations_for_pow_mre*(double)100);
					fclose(model_percent_mre);

					printf("\nModel validation completed\n");
					exit(0);
				}
				else if(active_threads == total_threads){ // Should restart the model 
					validation_pstate--;
					if(validation_pstate == lower_sampled_model_pstate)
						validation_pstate--;
					stopped_searching = 0;
					set_pstate(max_pstate);
    				set_threads(1);
    				best_throughput = -1;
					best_threads = 1;
					best_pstate = max_pstate;

					#ifdef DEBUG_HEURISTICS
						printf("Switched to: #threads %d - pstate %d\n", active_threads, current_pstate);
					#endif 
				}
				else{ // Not yet finished current P-state, should increase threads
					set_threads(active_threads+1);

					#ifdef DEBUG_HEURISTICS
						printf("Switched to: #threads %d - pstate %d\n", active_threads, current_pstate);
					#endif 
				}
			}
		} else if(detection_mode == 2){
				if(current_pstate == 0 && heuristic_mode != 10 && power > (power_limit*(1+(hysteresis/100))) ){
					#ifdef DEBUG_HEURISTICS
						printf("Disabling power boost\n");
					#endif

					set_pstate(1);
				}

				if(current_exploit_steps++ == exploit_steps){
					
					#ifdef DEBUG_HEURISTICS
						printf("EXPLORATION RESTARTED. PHASE 0 - INITIAL CONFIGURATION: #threads %d - p_state %d\n", best_threads, best_pstate);
					#endif
					
					if(heuristic_mode == 11){
						set_pstate(max_pstate);
						set_threads(starting_threads);
					}else if(heuristic_mode == 12 || heuristic_mode == 13 || heuristic_mode == 15){
						set_pstate(max_pstate);
						set_threads(1);
					}else{
						set_pstate(best_pstate);
						set_threads(best_threads);
						starting_threads = best_threads;
					}
					
					best_throughput = -1;
					best_pstate = -1; 
					best_threads = -1;
					best_power = -1;

					high_throughput = -1;
					high_pstate = -1;
					high_threads = -1;
					high_power = -1;

					low_throughput = -1;
					low_pstate = -1;
					low_threads = -1;
					low_power = -1;

					fluctuation_state = 0;
					window_time = 0;
					window_power = 0;
					current_window_slot = 0;

					stopped_searching = 0;

					min_thread_search = 1;
					max_thread_search = total_threads;

					min_pstate_search = 0; 
					max_pstate_search = max_pstate;

					min_thread_search_throughput = -1;
					max_thread_search_throughput = -1;
				}

				// Dynamic confiugration fluctuation used by dynamic_heuristic1
				if((heuristic_mode == 10 || heuristic_mode == 15 || heuristic_mode == 13 || heuristic_mode == 16) && stopped_searching){

					if(fluctuation_state == -1){
						low_power = power;
						low_throughput = throughput;
					}
					else if (fluctuation_state == 0){
						best_power = power;
						best_throughput = throughput;					}
					else if (fluctuation_state == 1){
						high_power = power;
						high_throughput = throughput;
					}
					else{
						printf("Invalid value of fluctuation_state. Aborting execution\n");
						exit(1);
					}

					//Update window_power, window_time and update current_window_slot
					window_power = (window_power*window_time+time*power)/(time+window_time);
					window_time+=time;
					current_window_slot++;

					#ifdef DEBUG_HEURISTICS
						printf("Window_power = %lf - Slot = %d - Fluctuation_state = %d", window_power, current_window_slot, fluctuation_state);
					#endif


					if(current_window_slot == window_size){ // Last slot in the current window 

						if(window_power < power_limit && best_power < power_limit && high_pstate > 0 && best_pstate > 0 && low_pstate > 0){
							high_pstate = high_pstate-1;
							best_pstate = best_pstate-1;
							low_pstate = low_pstate-1;
						} else if (window_power > power_limit && low_throughput > 0 && low_power > power_limit && low_pstate < max_pstate && best_pstate < max_pstate && high_pstate < max_pstate){
							high_pstate = high_pstate+1;
							best_pstate = best_pstate+1;
							low_pstate = low_pstate+1;
						} else if (low_throughput <= 0){
							if(best_pstate == max_pstate)
								low_pstate = best_pstate;
							else low_pstate = best_pstate+1;
							low_threads = best_threads;
							low_throughput = best_throughput;
						}
						else{
							if(high_throughput < best_throughput || (best_threads == high_threads && best_pstate == high_pstate)){
								if(best_pstate != 0 )
									high_pstate = best_pstate-1;
								else high_pstate = best_pstate;
								high_threads = best_threads;
								high_throughput = throughput;
							}

							if(low_throughput == 0 || (best_threads == low_threads && best_pstate == low_pstate)){
								if(best_pstate != max_pstate && best_power > power_limit)
									low_pstate = best_pstate+1;
								else low_pstate = best_pstate;
								low_threads = best_threads;
								low_throughput = best_throughput;
							}
						}

						set_threads(best_threads);
						set_pstate(best_pstate);
						fluctuation_state = 0;

						window_time = 0;
						window_power = 0;
						current_window_slot = 0;



						#ifdef DEBUG_HEURISTICS
							printf(" - Next configuration = BEST (restart)\n");
							printf("BEST - threads %d - pstate %d - power %lf\n", best_threads, best_pstate, best_power);
							printf("HIGH - threads %d - pstate %d - power %lf\n", high_threads, high_pstate, high_power);
							printf("LOW - threads %d - pstate %d - power %lf\n", low_threads, low_pstate, low_power);
						#endif
					}
					else{ // Regular slot, should decide configuration for next step 
						if(window_power < power_limit*(1-hysteresis/100)){	// Should increase_window_power
							if(best_power > power_limit || (high_throughput == -1)){
								set_threads(best_threads);
								set_pstate(best_pstate);
								fluctuation_state = 0;
								
								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = BEST\n");
								#endif
							}
							else {
								set_threads(high_threads);
								set_pstate(high_pstate);
								fluctuation_state = 1;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = HIGH\n");
								#endif
							}
						}else if (window_power > power_limit*(1+hysteresis/100)){ // Should decrease window_power
							if(best_power < power_limit || (low_throughput == -1)){
								set_threads(best_threads);
								set_pstate(best_pstate);
								fluctuation_state = 0;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = BEST\n");
								#endif
							}
							else {
								set_threads(low_threads);
								set_pstate(low_pstate);
								fluctuation_state = -1;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = LOW\n");
								#endif
							}
						}
						else{	// Window_power is within the hysteresis variation of window_power
							if(high_power < power_limit*(1+hysteresis/100) && high_throughput != -1){
								set_threads(high_threads);
								set_pstate(high_pstate);
								fluctuation_state = 1;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = HIGH (hysteresis)\n");
								#endif
							}else if(best_power < power_limit*(1+hysteresis/100) || low_throughput == -1){
								set_threads(best_threads);
								set_pstate(best_pstate);
								fluctuation_state = 0;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = BEST (hysteresis)\n");
								#endif
							}else{
								set_threads(low_threads);
								set_pstate(low_pstate);
								fluctuation_state = -1;

								#ifdef DEBUG_HEURISTICS
									printf(" - Next configuration = LOW (hysteresis)\n");
								#endif
							}
						}	
					}	
				}
			}
		}

		#ifdef DEBUG_OVERHEAD
			time_heuristic_end = get_time();
			time_heuristic_microseconds = (((double) time_heuristic_end) - ((double) time_heuristic_start))/1000;
			printf("DEBUG OVERHEAD - Inside heuristic(): %lf microseconds\n", time_heuristic_microseconds);
		#endif 
	}
