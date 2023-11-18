#!/bin/bash

if [ "$#" != 1 ]; then
    echo "Passed $# parameter(s) instead of 1"
    echo "usage: ./run_exp <05|06|07_and_08|09a|09b|09c|10|11|12|all>"
    exit
fi

mkdir -p data
mkdir -p figures_reproduced


atleast=0

if [[ "$1" == "05" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure05_run.sh
fi

if [[ "$1" == "06" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure06_run.sh
fi

if [[ "$1" == "07_and_08" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure07_and_08_run.sh
fi

if [[ "$1" == "09a" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure09a_run.sh
fi

if [[ "$1" == "09b" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure09b_run.sh
fi

if [[ "$1" == "09c" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure09c_run.sh
fi

if [[ "$1" == "10" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure10_run.sh
fi

if [[ "$1" == "11" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure11_run.sh
fi

if [[ "$1" == "12" || "$1" == "all" ]]; then
atleast=1
./scripts_run/figure12_run.sh
./scripts_run/table4_and_5_run.sh
fi

if [ "$atleast" == "0" ]; then
    echo "Passed $1 as parameter"
    echo "usage: ./run_exp <05|06|07_and_08|09a|09b|09c|10|11|12|all>"
    exit
fi
