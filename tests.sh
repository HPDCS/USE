#!/bin/bash
mkdir results
mkdir results/out

OUT_PHOLD_HS="results/out/out_phold-hs.txt" #_$(date +%d)-$(date +%m)-$(date +%Y)_$(date +%H):$(date +%M)"
touch ${OUT_PHOLD_HS}
./launch_phold-hs_retry.sh |tee ${OUT_PHOLD_HS}

OUT_TCAR="results/out/out_tcar.txt" #_$(date +%d)-$(date +%m)-$(date +%Y)_$(date +%H):$(date +%M)"
touch ${OUT_TCAR}
#./launch_tcar_retry.sh | tee ${OUT_PHOLD}

OUT_PHOLD="results/out/out_phold.txt" #_$(date +%d)-$(date +%m)-$(date +%Y)_$(date +%H):$(date +%M)"
touch ${OUT_PHOLD}
./launch_phold_retry.sh | tee ${TCAR}
