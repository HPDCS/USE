#!/bin/bash

mkdir -p use-debug
cd use-debug
rm -r *
cmake ../../.. -DCMAKE_BUILD_TYPE=Debug && make
cd ..
