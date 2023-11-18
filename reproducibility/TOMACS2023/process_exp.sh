#!/bin/bash

if [ "$#" != 1 ]; then
    echo "Passed $# parameter(s) instead of 1"
    echo "usage: ./process_data <05|06|07_and_08|09|10|11|12|all>"
    exit
fi

mkdir -p data
mkdir -p figures_reproduced


atleast=0

if [[ "$1" == "05" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure05_gen.sh
fi

if [[ "$1" == "06" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure06_gen.sh
fi

if [[ "$1" == "07_and_08" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure07_and_08_gen.sh
fi

if [[ "$1" == "09" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure09_gen.sh
./scripts_gen/table2_and_3_gen.sh
fi

if [[ "$1" == "10" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure10_gen.sh
fi

if [[ "$1" == "11" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure11_gen.sh
fi

if [[ "$1" == "12" || "$1" == "all" ]]; then
atleast=1
./scripts_gen/figure12_gen.sh
./scripts_gen/table4_and_5_gen.sh
fi

if [ "$atleast" == "0" ]; then
    echo "Passed $1 as parameter"
    echo "usage: ./process_data <05|06|07_and_08|09|10|11|12|all>"
    exit
fi
