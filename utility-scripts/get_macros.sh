#!/bin/bash

INVERTED="hpipe.h list  //#define  ( _H STAT_ LP_STATE_ atomic.h COLOR_ 
FETCH_AND_ _GNU_SOURCE __STACK_ _CAS __BITMAP TID INFTY ue_pool_size TOPOLOGY __LOCAL_INDEX_TYPES__
reverse.h topology.c ATOMIC_INC ATOMIC_DEC REMOVE_DEL REMOVE_DEL_INV _AECS segment.c buddy.c actual_index_top
current_LP_lvt BLOCKED_STATE ANTI_MSG NEW_EVT EXTRACTED ELIMINATED ANTI_MSG MASK ONE_EVT_PER_LP ST_BINDING_LP FULL_SPECULATIVE 
SUCCESS FAILURE INVALID_SOBJS INIT_ERROR INVALID_SOBJ MDT_RELEASE_FAIL "
grep "#define" src/ -nri > tmp 

for i in $INVERTED; do
    cat tmp $res | grep -v "$i" > tmp1
    rm tmp 
    mv tmp1 tmp
done

cat tmp
rm tmp

