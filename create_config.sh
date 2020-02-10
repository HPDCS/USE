# $1 number of starting threads
# $2 pstate
# $3 heuristic mode
# $4 power limit

f="powercap/config.txt"

echo "STARTING_THREADS=$1" 				>  $f
echo "STATIC_PSTATE=$2" 					>> $f
echo "POWER_LIMIT=$4" 					>> $f
echo "COMMITS_ROUND=10" 					>> $f
echo "ENERGY_PER_TX_LIMIT=50.000000" 	>> $f
echo "HEURISTIC_MODE=$3" 				>> $f
echo "JUMP_PERCENTAGE=10.000000" 		>> $f
echo "DETECTION_MODE=2" 					>> $f
echo "DETECTION_TP_THRESHOLD=10.000000" 	>> $f
echo "DETECTION_PWR_THRESHOLD=10.000000" >> $f
echo "EXPLOIT_STEPS=250" 				 >> $f
echo "EXTRA_RANGE_PERCENTAGE=10.000000" 	>> $f
echo "WINDOW_SIZE=10" 					>> $f
echo "HYSTERESIS=1.000000" 				>> $f
echo "POWER_UNCORE=0.5" 					>> $f
echo "CORE_PACKING=0" 					>> $f
echo "LOWER_SAMPLED_MODEL_PSTATE=2" 		>> $f
