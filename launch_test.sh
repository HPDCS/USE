test="pcs"

make $test
mv $test ${test}_sl

make $test NBC=1
mv $test ${test}_lf

for run in 1 2
do
    for lp in 16 64 256 1024
		do
		    for threads in 1 4 8 16 24 32
			do
				echo "./${test}_sl $threads $lp > log_test/${test}-sl-dymelor-nohijacker-$threads-$lp-$run"
				./${test}_sl $threads $lp > log_test/${test}-sl-dymelor-nohijacker-$threads-$lp-$run
				echo "./${test}_lf $threads $lp > log_test/${test}-lf-dymelor-nohijacker-$threads-$lp-$run"
				./${test}_lf $threads $lp > log_test/${test}-lf-dymelor-nohijacker-$threads-$lp-$run
			done
		done
done
