for loop_count in 50 #100 150
do
	for fan_out in 1 #10 25 50
	do
		for lookahead in 0.1 #0.01 0.001 0.0001
		do
			for test in phold 
			do
				make $test 			LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
				mv $test ${test}_sl_nohi
                
				make $test NBC=1 	LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
				mv $test ${test}_lf_nohi
				
				make $test 			REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
				mv $test ${test}_sl_hi
                                    
				make $test NBC=1 	REVERSIBLE=1 LOOKAHEAD=${lookahead} FAN_OUT=${fan_out} LOOP_COUNT=${loop_count}
				mv $test ${test}_lf_hi
				for run in 1 #2 
				do
					for lp in 64 #1024 #16 64 256 1024
						do
							for threads in 8 #1 4 8 16 24 32
							do
								echo "./${test}_sl_nohi $threads $lp 2>&1 1 > log_test/${test}-sl-dymelor-nohijacker-$threads-$lp-$run "
									  ./${test}_sl_nohi $threads $lp 2>&1 1 > log_test/${test}-sl-dymelor-nohijacker-$threads-$lp-$run
								echo "./${test}_lf_nohi $threads $lp 2>&1 1 > log_test/${test}-lf-dymelor-nohijacker-$threads-$lp-$run"
									  ./${test}_lf_nohi $threads $lp 2>&1 1 > log_test/${test}-lf-dymelor-nohijacker-$threads-$lp-$run
								echo "./${test}_sl_hi   $threads $lp 2>&1 1 > log_test/${test}-sl-dymelor-hijacker-$threads-$lp-$run  "
									  ./${test}_sl_hi   $threads $lp 2>&1 1 > log_test/${test}-sl-dymelor-hijacker-$threads-$lp-$run  
								echo "./${test}_lf_hi   $threads $lp 2>&1 1 > log_test/${test}-lf-dymelor-hijacker-$threads-$lp-$run  "
									  ./${test}_lf_hi   $threads $lp 2>&1 1 > log_test/${test}-lf-dymelor-hijacker-$threads-$lp-$run  
							done
						done
				done
				#rm ${test}_sl_nohi
                #rm ${test}_lf_nohi
				#rm ${test}_sl_hi
                #rm ${test}_lf_hi
			done
		done
	done
done
