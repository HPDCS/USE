#!/bin/bash

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]
then
	echo "Missing arguments. Try with: ./launch_pcs_hot_cells.sh <CPUs> <LPs> <SECs>"
else

CPUs=$1
LPs=$2
SECs=$3

ta_arrival=(  0.25   0.5   1.0   2.0   4.0  )
ta_duration=( 50     75    100   125   150  )
ta_change=(   50     75    100   125   150  )
ta_hotness=(  100    500   1000  2000  5000 )

hot_cell_factor=( 5 10 25 50 )
start_call_hot_factor=( 0.25 0.5 0.75 1.0 )
call_duration_hot_factor=( 0.5 0.75 1.0 1.5 )
handoff_leave_hot_factor=( 0.5 1.0 2.0 4.0 )

echo "PCS_HOT_CELLS (${CPUs};${LPs};${SECs})"
echo "PCS_HOT_CELLS (${CPUs};${LPs};${SECs})" > pcs_hc_res.txt

for taa in ${ta_arrival[@]}; do
for tad in ${ta_duration[@]}; do
for tac in ${ta_change[@]}; do
for tah in ${ta_hotness[@]}; do
for hcf in ${hot_cell_factor[@]}; do
for schf in ${start_call_hot_factor[@]}; do
for cdhf in ${call_duration_hot_factor[@]}; do
for hlhf in ${handoff_leave_hot_factor[@]}; do

datetime=`date '+%d/%m/%Y %H:%M:%S'`

make -B pcs_hot_cells TA=${taa} TA_DURATION=${tad} TA_CHANGE=${tac} TA_HOTNESS=${tah} HOT_CELL_FACTOR=${hcf} \
	START_CALL_HOT_FACTOR=${schf} CALL_DURATION_HOT_FACTOR=${cdhf} HANDOFF_LEAVE_HOT_FACTOR=${hlhf} \
		IPI_SUPPORT=1 POSTING=1 CHECKPOINT_PERIOD=10 > /dev/null && echo "--------------------" >> pcs_hc_res.txt && \
			echo "[${datetime}] (${taa};${tad};${tac};${tah};${hcf};${schf};${cdhf};${hlhf})" && \
				echo "[${datetime}] (${taa};${tad};${tac};${tah};${hcf};${schf};${cdhf};${hlhf})" >> pcs_hc_res.txt && \
					./pcs_hot_cells ${CPUs} ${LPs} ${SECs} | grep -e "Straggler\|Anti" >> pcs_hc_res.txt && echo "--------------------" >> pcs_hc_res.txt

sleep 1

done
done
done
done
done
done
done
done

fi